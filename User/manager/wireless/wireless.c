/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager and assembly provider.
* @details  : Provides the FC41D project binding for BLE advertising, BLE info
*             query, and non-blocking periodic service.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "usart.h"

#include "drvgpio.h"
#include "drvgpio_port.h"
#include "drvuart.h"

#include "../../../rep/comm/flowparser/flowparser_stream.h"
#include "../../../rep/module/fc41d/fc41d_assembly.h"
#include "../../../rep/service/console/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/ringbuffer/ringbuffer.h"
#include "../../port/drvuart_port.h"

static const char *const gWirelessBleInitCmdSeq[] = {
    "AT+QBLEINIT=2",
    "AT+QBLENAME=rumi",
    "AT+QBLEGATTSSRV=FE60",
    "AT+QBLEGATTSCHAR=FE61",
    "AT+QBLEGATTSCHAR=FE62",
};

typedef enum eWirelessInitQueryState {
    eWIRELESS_INIT_QUERY_WAIT_BLE_READY = 0,
    eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT,
    eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT,
    eWIRELESS_INIT_QUERY_VERSION_SUBMIT,
    eWIRELESS_INIT_QUERY_VERSION_WAIT,
    eWIRELESS_INIT_QUERY_DONE,
} eWirelessInitQueryState;

static eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
static bool gWirelessInitDone = false;
static bool gWirelessTransportPrepared = false;
static bool gWirelessBootReady = false;
static uint8_t gWirelessBootReadyMatchLen = 0U;
static uint32_t gWirelessBootWaitStartTick = 0U;
static eWirelessInitQueryState gWirelessInitQueryState = eWIRELESS_INIT_QUERY_WAIT_BLE_READY;
static stFc41dAtResp gWirelessBleAddrResp;
static stFc41dAtResp gWirelessVersionResp;
static bool gWirelessBleAddrLogged = false;
static bool gWirelessVersionLogged = false;

static stRingBuffer gWirelessAtRxRb;
static uint8_t gWirelessAtRxStorage[WIRELESS_AT_RX_CAPACITY];
static uint8_t gWirelessAtLineBuf[WIRELESS_AT_LINE_BUF_SIZE];
static uint8_t gWirelessAtCmdBuf[WIRELESS_AT_CMD_BUF_SIZE];
static uint8_t gWirelessAtPayloadBuf[WIRELESS_AT_PAYLOAD_BUF_SIZE];
static uint8_t gWirelessBleRxStorage[WIRELESS_BLE_RX_CAPACITY];
static uint8_t gWirelessUnusedRxStorage[WIRELESS_UNUSED_RX_CAPACITY];
static uint8_t gWirelessBleAddrLineBuf[WIRELESS_AT_LINE_BUF_SIZE];
static uint8_t gWirelessVersionLineBuf[WIRELESS_AT_LINE_BUF_SIZE];
static uint8_t gWirelessBleFrameBuf[WIRELESS_BLE_FRAME_MAX_LEN];
static uint16_t gWirelessBleFrameFill = 0U;
static uint16_t gWirelessBleFrameExpectedLen = 0U;

#define WIRELESS_INFO_QUERY_TIMEOUT_MS      3000U

static void wirelessDelayMs(uint32_t delayMs);
static void wirelessResetBleFrameCollector(void);
static bool wirelessCollectRawBleByte(uint8_t byte);
static void wirelessResetBootWaitState(void);
static bool wirelessConsumeBootReadyByte(uint8_t byte);
static bool wirelessWaitBootReady(void);
static bool wirelessIsBleReadyForInitQuery(eFc41dBleState state);
static bool wirelessShouldIgnoreInfoLine(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessBleAddrLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessVersionLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static eFc41dStatus wirelessStartBleAddrQuery(void);
static eFc41dStatus wirelessStartVersionQuery(void);
static void wirelessFinalizeBleAddrQuery(void);
static void wirelessFinalizeVersionQuery(void);
static void wirelessProcessInitQueries(void);
static void wirelessUpdateState(void);
static eDrvStatus wirelessFc41dSend(const uint8_t *buf, uint16_t len, void *userCtx);
static bool wirelessRouteLinePayload(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payloadBuf, uint16_t *payloadLen);

static void wirelessDelayMs(uint32_t delayMs)
{
    if (delayMs == 0U) {
        return;
    }

    if (repRtosIsSchedulerRunning()) {
        (void)repRtosDelayMs(delayMs);
        return;
    }

    HAL_Delay(delayMs);
}

static void wirelessResetBleFrameCollector(void)
{
    gWirelessBleFrameFill = 0U;
    gWirelessBleFrameExpectedLen = 0U;
}

static bool wirelessCollectRawBleByte(uint8_t byte)
{
    uint16_t lPayloadLen;
    stRingBuffer *lBleRxRb;

    if (gWirelessBleFrameFill == 0U) {
        if (byte != 0xFAU) {
            return false;
        }

        gWirelessBleFrameBuf[0] = byte;
        gWirelessBleFrameFill = 1U;
        gWirelessBleFrameExpectedLen = 0U;
        return true;
    }

    if (gWirelessBleFrameFill >= sizeof(gWirelessBleFrameBuf)) {
        wirelessResetBleFrameCollector();
        return false;
    }

    gWirelessBleFrameBuf[gWirelessBleFrameFill++] = byte;
    if (gWirelessBleFrameFill == 2U) {
        if (gWirelessBleFrameBuf[1] != 0xFCU) {
            wirelessResetBleFrameCollector();
            if (byte == 0xFAU) {
                gWirelessBleFrameBuf[0] = byte;
                gWirelessBleFrameFill = 1U;
                return true;
            }
            return false;
        }
        return true;
    }

    if (gWirelessBleFrameFill == 6U) {
        lPayloadLen = (uint16_t)(((uint16_t)gWirelessBleFrameBuf[4] << 8U) | gWirelessBleFrameBuf[5]);
        gWirelessBleFrameExpectedLen = (uint16_t)(lPayloadLen + 8U);
        if ((gWirelessBleFrameExpectedLen < 8U) || (gWirelessBleFrameExpectedLen > sizeof(gWirelessBleFrameBuf))) {
            wirelessResetBleFrameCollector();
            return false;
        }
    }

    if ((gWirelessBleFrameExpectedLen != 0U) && (gWirelessBleFrameFill >= gWirelessBleFrameExpectedLen)) {
        lBleRxRb = fc41dBleGetRxRingBuffer(WIRELESS_FC41D_DEVICE);
        if ((lBleRxRb == NULL) ||
            (ringBufferWrite(lBleRxRb, gWirelessBleFrameBuf, gWirelessBleFrameExpectedLen) != gWirelessBleFrameExpectedLen)) {
            LOG_W(WIRELESS_LOG_TAG, "ble raw frame drop len=%u", (unsigned int)gWirelessBleFrameExpectedLen);
        }
        wirelessResetBleFrameCollector();
    }

    return true;
}

static void wirelessResetBootWaitState(void)
{
    gWirelessBootReady = false;
    gWirelessBootReadyMatchLen = 0U;
    gWirelessBootWaitStartTick = fc41dPlatformGetTickMs();
    gWirelessInitQueryState = eWIRELESS_INIT_QUERY_WAIT_BLE_READY;
    gWirelessBleAddrLogged = false;
    gWirelessVersionLogged = false;
    wirelessResetBleFrameCollector();
    (void)memset(&gWirelessBleAddrResp, 0, sizeof(gWirelessBleAddrResp));
    (void)memset(&gWirelessVersionResp, 0, sizeof(gWirelessVersionResp));
}

static bool wirelessConsumeBootReadyByte(uint8_t byte)
{
    static const char lReadyToken[] = "ready";
    uint8_t lLowerByte;

    lLowerByte = (uint8_t)tolower((int)byte);
    if (lLowerByte == (uint8_t)lReadyToken[gWirelessBootReadyMatchLen]) {
        gWirelessBootReadyMatchLen++;
    } else if (lLowerByte == (uint8_t)lReadyToken[0]) {
        gWirelessBootReadyMatchLen = 1U;
    } else {
        gWirelessBootReadyMatchLen = 0U;
    }

    if (gWirelessBootReadyMatchLen >= (uint8_t)(sizeof(lReadyToken) - 1U)) {
        gWirelessBootReadyMatchLen = 0U;
        return true;
    }

    return false;
}

static bool wirelessWaitBootReady(void)
{
    uint8_t lByte;

    if (gWirelessBootReady) {
        return true;
    }

    fc41dPlatformPollRx(WIRELESS_FC41D_DEVICE);
    while (ringBufferRead(&gWirelessAtRxRb, &lByte, 1U) == 1U) {
        if (wirelessConsumeBootReadyByte(lByte)) {
            gWirelessBootReady = true;
            LOG_I(WIRELESS_LOG_TAG, "fc41d boot ready");
            return true;
        }
    }

    if ((uint32_t)(fc41dPlatformGetTickMs() - gWirelessBootWaitStartTick) >= WIRELESS_BOOT_READY_TIMEOUT_MS) {
        gWirelessState = eWIRELESS_STATE_ERROR;
    }

    return false;
}

static bool wirelessIsBleReadyForInitQuery(eFc41dBleState state)
{
    switch (state) {
        case FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT:
        case FC41D_BLE_STATE_PERIPHERAL_CONNECTED:
        case FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED:
        case FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT:
        case FC41D_BLE_STATE_CENTRAL_CONNECTED:
        case FC41D_BLE_STATE_CENTRAL_DISCONNECTED:
            return true;

        default:
            return false;
    }
}

static bool wirelessShouldIgnoreInfoLine(const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return true;
    }

    if ((lineLen >= 2U) && (lineBuf[0] == 'A') && (lineBuf[1] == 'T')) {
        return true;
    }

    if (((lineLen == 2U) && (memcmp(lineBuf, "OK", 2U) == 0)) ||
        ((lineLen == 5U) && (memcmp(lineBuf, "ERROR", 5U) == 0))) {
        return true;
    }

    return false;
}

static void wirelessBleAddrLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    bool *lLogged = (bool *)userData;

    if (wirelessShouldIgnoreInfoLine(lineBuf, lineLen)) {
        return;
    }

    if (lLogged != NULL) {
        *lLogged = true;
    }

    LOG_I(WIRELESS_LOG_TAG, "ble mac: %.*s", (int)lineLen, (const char *)lineBuf);
}

static void wirelessVersionLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    bool *lLogged = (bool *)userData;

    if (wirelessShouldIgnoreInfoLine(lineBuf, lineLen)) {
        return;
    }

    if (lLogged != NULL) {
        *lLogged = true;
    }

    LOG_I(WIRELESS_LOG_TAG, "fc41d version: %.*s", (int)lineLen, (const char *)lineBuf);
}

static eFc41dStatus wirelessStartBleAddrQuery(void)
{
    stFc41dAtOpt lOpt;
    char lCmdBuf[32U];
    eFc41dStatus lStatus;

    (void)memset(&lOpt, 0, sizeof(lOpt));
    (void)memset(&gWirelessBleAddrResp, 0, sizeof(gWirelessBleAddrResp));
    gWirelessBleAddrResp.lineBuf = gWirelessBleAddrLineBuf;
    gWirelessBleAddrResp.lineBufSize = (uint16_t)sizeof(gWirelessBleAddrLineBuf);
    gWirelessBleAddrResp.pfLineCallback = wirelessBleAddrLineCallback;
    gWirelessBleAddrResp.lineCallbackUserData = &gWirelessBleAddrLogged;
    lOpt.totalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.responseToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.finalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;

    lStatus = fc41dAtBuildQueryCmd(lCmdBuf, (uint16_t)sizeof(lCmdBuf), FC41D_AT_CATALOG_CMD_QBLEADDR);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dExecAtText(WIRELESS_FC41D_DEVICE, lCmdBuf, &lOpt, &gWirelessBleAddrResp);
    if (lStatus == FC41D_STATUS_OK) {
        LOG_I(WIRELESS_LOG_TAG, "ble mac query start");
    }

    return lStatus;
}

static eFc41dStatus wirelessStartVersionQuery(void)
{
    stFc41dAtOpt lOpt;
    eFc41dStatus lStatus;

    (void)memset(&lOpt, 0, sizeof(lOpt));
    (void)memset(&gWirelessVersionResp, 0, sizeof(gWirelessVersionResp));
    gWirelessVersionResp.lineBuf = gWirelessVersionLineBuf;
    gWirelessVersionResp.lineBufSize = (uint16_t)sizeof(gWirelessVersionLineBuf);
    gWirelessVersionResp.pfLineCallback = wirelessVersionLineCallback;
    gWirelessVersionResp.lineCallbackUserData = &gWirelessVersionLogged;
    lOpt.totalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.responseToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.finalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;

    lStatus = fc41dExecAtCmd(WIRELESS_FC41D_DEVICE, FC41D_AT_CATALOG_CMD_QVERSION, &lOpt, &gWirelessVersionResp);
    if (lStatus == FC41D_STATUS_OK) {
        LOG_I(WIRELESS_LOG_TAG, "fc41d version query start");
    }

    return lStatus;
}

static void wirelessFinalizeBleAddrQuery(void)
{
    if (gWirelessBleAddrResp.result == FC41D_AT_RESULT_OK) {
        if (!gWirelessBleAddrLogged && (gWirelessBleAddrResp.lineLen > 0U)) {
            LOG_I(WIRELESS_LOG_TAG,
                  "ble mac: %.*s",
                  (int)gWirelessBleAddrResp.lineLen,
                  (const char *)gWirelessBleAddrResp.lineBuf);
        }
        return;
    }

    LOG_W(WIRELESS_LOG_TAG,
          "ble mac query fail result=%u lastLine=%.*s",
          (unsigned int)gWirelessBleAddrResp.result,
          (int)gWirelessBleAddrResp.lineLen,
          (const char *)gWirelessBleAddrResp.lineBuf);
}

static void wirelessFinalizeVersionQuery(void)
{
    if (gWirelessVersionResp.result == FC41D_AT_RESULT_OK) {
        if (!gWirelessVersionLogged && (gWirelessVersionResp.lineLen > 0U)) {
            LOG_I(WIRELESS_LOG_TAG,
                  "fc41d version: %.*s",
                  (int)gWirelessVersionResp.lineLen,
                  (const char *)gWirelessVersionResp.lineBuf);
        }
        return;
    }

    LOG_W(WIRELESS_LOG_TAG,
          "fc41d version query fail result=%u lastLine=%.*s",
          (unsigned int)gWirelessVersionResp.result,
          (int)gWirelessVersionResp.lineLen,
          (const char *)gWirelessVersionResp.lineBuf);
}

static void wirelessProcessInitQueries(void)
{
    stFc41dInfo lInfo;
    eFc41dStatus lStatus;

    if (gWirelessInitQueryState == eWIRELESS_INIT_QUERY_DONE) {
        return;
    }

    if (fc41dGetInfo(WIRELESS_FC41D_DEVICE, &lInfo) != FC41D_STATUS_OK) {
        return;
    }

    switch (gWirelessInitQueryState) {
        case eWIRELESS_INIT_QUERY_WAIT_BLE_READY:
            if (wirelessIsBleReadyForInitQuery(lInfo.ble.state)) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT:
            lStatus = wirelessStartBleAddrQuery();
            if (lStatus == FC41D_STATUS_OK) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT;
            } else if (lStatus != FC41D_STATUS_BUSY) {
                LOG_W(WIRELESS_LOG_TAG, "ble mac query submit fail status=%u", (unsigned int)lStatus);
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT:
            if (!fc41dExecAtIsBusy(WIRELESS_FC41D_DEVICE)) {
                wirelessFinalizeBleAddrQuery();
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_VERSION_SUBMIT:
            lStatus = wirelessStartVersionQuery();
            if (lStatus == FC41D_STATUS_OK) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_WAIT;
            } else if (lStatus != FC41D_STATUS_BUSY) {
                LOG_W(WIRELESS_LOG_TAG, "fc41d version query submit fail status=%u", (unsigned int)lStatus);
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_DONE;
            }
            break;

        case eWIRELESS_INIT_QUERY_VERSION_WAIT:
            if (!fc41dExecAtIsBusy(WIRELESS_FC41D_DEVICE)) {
                wirelessFinalizeVersionQuery();
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_DONE;
            }
            break;

        case eWIRELESS_INIT_QUERY_DONE:
        default:
            break;
    }
}

static void wirelessUpdateState(void)
{
    stFc41dInfo lInfo;

    if (!gWirelessBootReady) {
        if (gWirelessState != eWIRELESS_STATE_ERROR) {
            gWirelessState = eWIRELESS_STATE_INIT;
        }
        return;
    }

    if (fc41dGetInfo(WIRELESS_FC41D_DEVICE, &lInfo) != FC41D_STATUS_OK) {
        if (gWirelessInitDone) {
            gWirelessState = eWIRELESS_STATE_ERROR;
        }
        return;
    }

    if (lInfo.ble.state == FC41D_BLE_STATE_ERROR) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return;
    }

    gWirelessState = lInfo.isReady ? eWIRELESS_STATE_NORMAL : eWIRELESS_STATE_INIT;
}

static eDrvStatus wirelessFc41dSend(const uint8_t *buf, uint16_t len, void *userCtx)
{
    (void)userCtx;

    if ((buf == NULL) || (len == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartTransmit(DRVUART_DEBUG, buf, len, 100U);
}

static bool wirelessRouteLinePayload(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
    uint16_t lIndex;

    if ((lineBuf == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return false;
    }

    for (lIndex = 0U; lIndex < lineLen; lIndex++) {
        if (lineBuf[lIndex] == ':') {
            lIndex++;
            while ((lIndex < lineLen) && ((lineBuf[lIndex] == ' ') || (lineBuf[lIndex] == '\t'))) {
                lIndex++;
            }
            break;
        }
    }

    if (lIndex >= lineLen) {
        *payloadBuf = lineBuf;
        *payloadLen = lineLen;
        return true;
    }

    *payloadBuf = &lineBuf[lIndex];
    *payloadLen = (uint16_t)(lineLen - lIndex);
    return true;
}

bool wirelessInit(void)
{
    stFc41dCfg lCfg;

    if (gWirelessInitDone) {
        return gWirelessState != eWIRELESS_STATE_ERROR;
    }

    if (drvUartInit(DRVUART_DEBUG) != DRV_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        LOG_E(WIRELESS_LOG_TAG, "debug uart init fail");
        return false;
    }

    if (fc41dGetDefCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dSetCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dInit(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    wirelessResetBootWaitState();
    gWirelessInitDone = true;
    wirelessUpdateState();
    LOG_I(WIRELESS_LOG_TAG, "fc41d init ok ble=rumi");
    return true;
}

void wirelessProcess(void)
{
    if (!gWirelessInitDone) {
        return;
    }

    if (!wirelessWaitBootReady()) {
        return;
    }

    if (fc41dProcess(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        (void)fc41dRecover(WIRELESS_FC41D_DEVICE);
        return;
    }

    wirelessUpdateState();
    wirelessProcessInitQueries();
}

const eWirelessState *wirelessGetStatus(void)
{
    return &gWirelessState;
}

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->ble.enableRx = true;
    cfg->ble.rxOverwriteOnFull = true;
    cfg->ble.workMode = FC41D_BLE_WORK_MODE_PERIPHERAL;
    cfg->ble.initCmdSeq = gWirelessBleInitCmdSeq;
    cfg->ble.initCmdSeqLen = (uint8_t)(sizeof(gWirelessBleInitCmdSeq) / sizeof(gWirelessBleInitCmdSeq[0]));
    cfg->wifi.enableRx = false;
    cfg->wifi.rxOverwriteOnFull = false;
    cfg->wifi.workMode = FC41D_WIFI_WORK_MODE_DISABLED;
    cfg->execGuardMs = 5000U;
    cfg->bootMode = FC41D_MODE_COMMAND;
}

bool fc41dPlatformIsValidAssemble(eFc41dMapType device)
{
    (void)device;
    return huart4.Instance != NULL;
}

uint32_t fc41dPlatformGetTickMs(void)
{
    return repRtosIsSchedulerRunning() ? repRtosGetTickMs() : HAL_GetTick();
}

eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device)
{
    (void)device;

    if ((huart4.Instance == NULL) || (drvUartInit(DRVUART_DEBUG) != DRV_STATUS_OK)) {
        return FC41D_STATUS_NOT_READY;
    }

    if (!gWirelessTransportPrepared) {
        drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_RESET);
        wirelessDelayMs(WIRELESS_RESET_ASSERT_MS);
        drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_SET);
        wirelessDelayMs(WIRELESS_RESET_RELEASE_MS);
        gWirelessTransportPrepared = true;
    }

    wirelessResetBootWaitState();
    return FC41D_STATUS_OK;
}

void fc41dPlatformPollRx(eFc41dMapType device)
{
    uint8_t lByte;
    uint16_t lPendingLength;

    (void)device;
    if (huart4.Instance == NULL) {
        return;
    }

    lPendingLength = drvUartGetDataLen(DRVUART_DEBUG);
    while (lPendingLength > 0U) {
        if (drvUartReceive(DRVUART_DEBUG, &lByte, 1U) != DRV_STATUS_OK) {
            break;
        }

        if (wirelessCollectRawBleByte(lByte)) {
            lPendingLength--;
            continue;
        }

        if (ringBufferWrite(&gWirelessAtRxRb, &lByte, 1U) != 1U) {
            break;
        }

        lPendingLength--;
    }
}

eFc41dStatus fc41dPlatformInitRxBuffers(eFc41dMapType device, stRingBuffer *bleRxRb, stRingBuffer *wifiRxRb)
{
    (void)device;

    if ((bleRxRb == NULL) || (wifiRxRb == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    if ((ringBufferInit(bleRxRb, gWirelessBleRxStorage, sizeof(gWirelessBleRxStorage)) != RINGBUFFER_OK) ||
        (ringBufferInit(wifiRxRb, gWirelessUnusedRxStorage, sizeof(gWirelessUnusedRxStorage)) != RINGBUFFER_OK)) {
        return FC41D_STATUS_ERROR;
    }

    return FC41D_STATUS_OK;
}

eFlowParserStrmSta fc41dPlatformInitAtStream(eFc41dMapType device, stFlowParserStream *stream,
                                             flowparserStreamLineFunc urcHandler, void *urcUserCtx)
{
    stFlowParserStreamCfg lCfg;
    static const char *const lUrcPatterns[] = {
        "+BLE*",
        "+QBLE*",
    };

    (void)device;
    if (stream == NULL) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    if (ringBufferInit(&gWirelessAtRxRb, gWirelessAtRxStorage, sizeof(gWirelessAtRxStorage)) != RINGBUFFER_OK) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memset(&lCfg, 0, sizeof(lCfg));
    lCfg.tokCfg.ringBuf = &gWirelessAtRxRb;
    lCfg.tokCfg.lineBuf = gWirelessAtLineBuf;
    lCfg.tokCfg.lineBufSize = sizeof(gWirelessAtLineBuf);
    lCfg.cmdBuf = gWirelessAtCmdBuf;
    lCfg.cmdBufSize = sizeof(gWirelessAtCmdBuf);
    lCfg.payloadBuf = gWirelessAtPayloadBuf;
    lCfg.payloadBufSize = sizeof(gWirelessAtPayloadBuf);
    lCfg.send = wirelessFc41dSend;
    lCfg.portUserCtx = &huart4;
    lCfg.getTick = fc41dPlatformGetTickMs;
    lCfg.urcPatterns = lUrcPatterns;
    lCfg.urcPatternCnt = (uint8_t)(sizeof(lUrcPatterns) / sizeof(lUrcPatterns[0]));
    lCfg.urcHandler = urcHandler;
    lCfg.urcUserCtx = urcUserCtx;
    lCfg.procBudget = 8U;
    return flowparserStreamInit(stream, &lCfg);
}

bool fc41dPlatformRouteLine(eFc41dMapType device, const uint8_t *lineBuf, uint16_t lineLen,
                            eFc41dRxChannel *channel, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
    (void)device;

    if ((lineBuf == NULL) || (channel == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return false;
    }

    if ((lineLen >= 4U) && (memcmp(lineBuf, "+BLE", 4U) == 0)) {
        *channel = FC41D_RX_CHANNEL_BLE;
        return wirelessRouteLinePayload(lineBuf, lineLen, payloadBuf, payloadLen);
    }

    return false;
}

/**************************End of file********************************/
