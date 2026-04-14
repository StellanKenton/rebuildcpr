/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager and assembly provider.
* @details  : Provides the FC41D project binding for WiFi auto-connect, TCP
*             server setup, BLE advertising, and non-blocking periodic service.
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

static const char *const gWirelessWifiInitCmdSeq[] = {
    "AT+QSTAAPINFODEF=\"rumi\",\"1234567890\"",
};

static const char *const gWirelessWifiStartCmdSeq[] = {
    "AT+QIOPEN=0,\"TCP SERVER\",5000",
    "AT+QIACCEPT=0,1",
};

static eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
static bool gWirelessInitDone = false;
static bool gWirelessTransportPrepared = false;
static bool gWirelessBootReady = false;
static uint8_t gWirelessBootReadyMatchLen = 0U;
static uint32_t gWirelessBootWaitStartTick = 0U;

static stRingBuffer gWirelessAtRxRb;
static uint8_t gWirelessAtRxStorage[WIRELESS_AT_RX_CAPACITY];
static uint8_t gWirelessAtLineBuf[WIRELESS_AT_LINE_BUF_SIZE];
static uint8_t gWirelessAtCmdBuf[WIRELESS_AT_CMD_BUF_SIZE];
static uint8_t gWirelessAtPayloadBuf[WIRELESS_AT_PAYLOAD_BUF_SIZE];
static uint8_t gWirelessBleRxStorage[WIRELESS_BLE_RX_CAPACITY];
static uint8_t gWirelessWifiRxStorage[WIRELESS_WIFI_RX_CAPACITY];

static void wirelessDelayMs(uint32_t delayMs);
static void wirelessResetBootWaitState(void);
static bool wirelessConsumeBootReadyByte(uint8_t byte);
static bool wirelessWaitBootReady(void);
static void wirelessUpdateState(void);
static const char *wirelessGetWifiStateName(eFc41dWifiState state);
static bool wirelessIsWifiDiagLine(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessLogWifiDiagLine(const uint8_t *lineBuf, uint16_t lineLen);
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

static void wirelessResetBootWaitState(void)
{
    gWirelessBootReady = false;
    gWirelessBootReadyMatchLen = 0U;
    gWirelessBootWaitStartTick = fc41dPlatformGetTickMs();
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

static void wirelessUpdateState(void)
{
    stFc41dInfo lInfo;
    static bool lWifiStateLogged = false;
    static eFc41dWifiState lLastWifiState = FC41D_WIFI_STATE_IDLE;
    static uint8_t lLastReconnectRetryCount = 0U;

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

    if ((lInfo.ble.state == FC41D_BLE_STATE_ERROR) || (lInfo.wifi.state == FC41D_WIFI_STATE_ERROR)) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return;
    }

    if ((!lWifiStateLogged) || (lLastWifiState != lInfo.wifi.state) ||
        (lLastReconnectRetryCount != lInfo.wifi.reconnectRetryCount)) {
        LOG_I(WIRELESS_LOG_TAG,
              "wifi state=%s retries=%u routed=%lu dropped=%lu",
              wirelessGetWifiStateName(lInfo.wifi.state),
              (unsigned int)lInfo.wifi.reconnectRetryCount,
              (unsigned long)lInfo.wifi.rxRoutedBytes,
              (unsigned long)lInfo.wifi.rxDroppedBytes);
        lLastWifiState = lInfo.wifi.state;
        lLastReconnectRetryCount = lInfo.wifi.reconnectRetryCount;
        lWifiStateLogged = true;
    }

    gWirelessState = lInfo.isReady ? eWIRELESS_STATE_NORMAL : eWIRELESS_STATE_INIT;
}

static const char *wirelessGetWifiStateName(eFc41dWifiState state)
{
    switch (state) {
        case FC41D_WIFI_STATE_IDLE:
            return "idle";
        case FC41D_WIFI_STATE_STA_INIT:
            return "sta_init";
        case FC41D_WIFI_STATE_STA_CONNECTING:
            return "sta_connecting";
        case FC41D_WIFI_STATE_STA_SERVICE_STARTING:
            return "sta_service_starting";
        case FC41D_WIFI_STATE_STA_CONNECTED:
            return "sta_connected";
        case FC41D_WIFI_STATE_STA_DISCONNECTED:
            return "sta_disconnected";
        case FC41D_WIFI_STATE_AP_INIT:
            return "ap_init";
        case FC41D_WIFI_STATE_AP_STARTING:
            return "ap_starting";
        case FC41D_WIFI_STATE_AP_ACTIVE:
            return "ap_active";
        case FC41D_WIFI_STATE_AP_STOPPED:
            return "ap_stopped";
        case FC41D_WIFI_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static bool wirelessIsWifiDiagLine(const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((lineBuf == NULL) || (lineLen < 4U) || (lineBuf[0] != '+')) {
        return false;
    }

    if (((lineLen >= 6U) && (memcmp(lineBuf, "+QSTA", 5U) == 0)) ||
        ((lineLen >= 4U) && (memcmp(lineBuf, "+STA", 4U) == 0)) ||
        ((lineLen >= 3U) && (memcmp(lineBuf, "+QI", 3U) == 0))) {
        return true;
    }

    return false;
}

static void wirelessLogWifiDiagLine(const uint8_t *lineBuf, uint16_t lineLen)
{
    if (!wirelessIsWifiDiagLine(lineBuf, lineLen)) {
        return;
    }

    LOG_I(WIRELESS_LOG_TAG, "wifi urc: %.*s", (int)lineLen, (const char *)lineBuf);
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
    LOG_I(WIRELESS_LOG_TAG,
          "fc41d init ok ble=rumi wifi=rumi tcpPort=%u",
          (unsigned int)WIRELESS_WIFI_TCP_SERVER_PORT);
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
    cfg->wifi.enableRx = true;
    cfg->wifi.rxOverwriteOnFull = true;
    cfg->wifi.workMode = FC41D_WIFI_WORK_MODE_STA;
    cfg->wifi.initCmdSeq = gWirelessWifiInitCmdSeq;
    cfg->wifi.initCmdSeqLen = (uint8_t)(sizeof(gWirelessWifiInitCmdSeq) / sizeof(gWirelessWifiInitCmdSeq[0]));
    cfg->wifi.startCmdSeq = gWirelessWifiStartCmdSeq;
    cfg->wifi.startCmdSeqLen = (uint8_t)(sizeof(gWirelessWifiStartCmdSeq) / sizeof(gWirelessWifiStartCmdSeq[0]));
    cfg->wifi.autoReconnect = true;
    cfg->wifi.reconnectIntervalMs = WIRELESS_WIFI_RECONNECT_MS;
    cfg->wifi.reconnectMaxRetries = 0U;
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
        (ringBufferInit(wifiRxRb, gWirelessWifiRxStorage, sizeof(gWirelessWifiRxStorage)) != RINGBUFFER_OK)) {
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
        "+WIFI*",
        "+QSTA*",
        "+STA*",
        "+AP*",
        "+QI*",
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

    wirelessLogWifiDiagLine(lineBuf, lineLen);

    if (((lineLen >= 5U) && (memcmp(lineBuf, "+WIFI", 5U) == 0)) ||
        ((lineLen >= 5U) && (memcmp(lineBuf, "+QSTA", 5U) == 0)) ||
        ((lineLen >= 4U) && (memcmp(lineBuf, "+STA", 4U) == 0)) ||
        ((lineLen >= 3U) && (memcmp(lineBuf, "+AP", 3U) == 0)) ||
        ((lineLen >= 3U) && (memcmp(lineBuf, "+QI", 3U) == 0))) {
        *channel = FC41D_RX_CHANNEL_WIFI;
        return wirelessRouteLinePayload(lineBuf, lineLen, payloadBuf, payloadLen);
    }

    return false;
}

/**************************End of file********************************/
