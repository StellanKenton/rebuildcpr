/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager and assembly provider.
* @details  : Provides the minimum FC41D integration required by the current
*             firmware build and keeps the manager non-blocking.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <stddef.h>
#include <string.h>

#include "usart.h"

#include "../../../rep/comm/flowparser/flowparser_stream.h"
#include "../../../rep/service/console/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/ringbuffer/ringbuffer.h"

#define WIRELESS_LOG_TAG                     "wireless"
#define WIRELESS_FC41D_DEVICE                FC41D_DEV0
#define WIRELESS_RESET_ASSERT_MS             20U
#define WIRELESS_RESET_RELEASE_MS            200U
#define WIRELESS_AT_RX_CAPACITY              512U
#define WIRELESS_AT_LINE_BUF_SIZE            256U
#define WIRELESS_AT_CMD_BUF_SIZE             256U
#define WIRELESS_AT_PAYLOAD_BUF_SIZE         256U
#define WIRELESS_BLE_RX_CAPACITY             512U
#define WIRELESS_WIFI_RX_CAPACITY            512U

static stWirelessStatus gWirelessStatus = {
    .state = eWIRELESS_STATE_UNINIT,
    .initStarted = false,
    .aliveCheckStarted = false,
};

static stRingBuffer gWirelessAtRxRb;
static uint8_t gWirelessAtRxStorage[WIRELESS_AT_RX_CAPACITY];
static uint8_t gWirelessAtLineBuf[WIRELESS_AT_LINE_BUF_SIZE];
static uint8_t gWirelessAtCmdBuf[WIRELESS_AT_CMD_BUF_SIZE];
static uint8_t gWirelessAtPayloadBuf[WIRELESS_AT_PAYLOAD_BUF_SIZE];
static uint8_t gWirelessBleRxStorage[WIRELESS_BLE_RX_CAPACITY];
static uint8_t gWirelessWifiRxStorage[WIRELESS_WIFI_RX_CAPACITY];
static bool gWirelessTransportPrepared = false;

static void wirelessDelayMs(uint32_t delayMs);
static void wirelessRefreshStatus(void);
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

static void wirelessRefreshStatus(void)
{
    (void)fc41dGetInfo(WIRELESS_FC41D_DEVICE, &gWirelessStatus.fc41dInfo);
    (void)fc41dGetTxnStatus(WIRELESS_FC41D_DEVICE, &gWirelessStatus.txnStatus);
}

static eDrvStatus wirelessFc41dSend(const uint8_t *buf, uint16_t len, void *userCtx)
{
    UART_HandleTypeDef *handle = (UART_HandleTypeDef *)userCtx;

    if ((handle == NULL) || (handle->Instance == NULL) || (buf == NULL) || (len == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return (HAL_UART_Transmit(handle, (uint8_t *)buf, len, 100U) == HAL_OK) ? DRV_STATUS_OK : DRV_STATUS_ERROR;
}

static bool wirelessRouteLinePayload(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
    uint16_t idx;

    if ((lineBuf == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return false;
    }

    for (idx = 0U; idx < lineLen; idx++) {
        if (lineBuf[idx] == ':') {
            idx++;
            while ((idx < lineLen) && ((lineBuf[idx] == ' ') || (lineBuf[idx] == '\t'))) {
                idx++;
            }
            break;
        }
    }

    if (idx >= lineLen) {
        *payloadBuf = lineBuf;
        *payloadLen = lineLen;
        return true;
    }

    *payloadBuf = &lineBuf[idx];
    *payloadLen = (uint16_t)(lineLen - idx);
    return true;
}

bool wirelessInit(void)
{
    stFc41dCfg cfg;

    if (gWirelessStatus.state == eWIRELESS_STATE_ACTIVE) {
        return true;
    }

    if (fc41dGetDefCfg(WIRELESS_FC41D_DEVICE, &cfg) != FC41D_STATUS_OK) {
        gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        return false;
    }

    cfg.bootMode = FC41D_MODE_COMMAND;
    cfg.ble.workMode = FC41D_BLE_WORK_MODE_DISABLED;
    cfg.wifi.workMode = FC41D_WIFI_WORK_MODE_DISABLED;

    if (fc41dSetCfg(WIRELESS_FC41D_DEVICE, &cfg) != FC41D_STATUS_OK) {
        gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        return false;
    }

    if (fc41dInit(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        return false;
    }

    gWirelessStatus.state = eWIRELESS_STATE_READY;
    gWirelessStatus.initStarted = true;
    gWirelessStatus.aliveCheckStarted = false;
    wirelessRefreshStatus();
    LOG_I(WIRELESS_LOG_TAG, "fc41d init ok");
    return true;
}

void wirelessProcess(void)
{
    eFc41dStatus status;

    if (gWirelessStatus.state == eWIRELESS_STATE_UNINIT) {
        return;
    }

    if (!fc41dIsReady(WIRELESS_FC41D_DEVICE)) {
        gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        return;
    }

    if (!gWirelessStatus.aliveCheckStarted) {
        status = fc41dAtCheckAlive(WIRELESS_FC41D_DEVICE);
        if (status == FC41D_STATUS_OK) {
            gWirelessStatus.aliveCheckStarted = true;
        } else if (status != FC41D_STATUS_BUSY) {
            gWirelessStatus.state = eWIRELESS_STATE_FAULT;
            wirelessRefreshStatus();
            return;
        }
    }

    status = fc41dProcess(WIRELESS_FC41D_DEVICE);
    wirelessRefreshStatus();
    if (status != FC41D_STATUS_OK) {
        gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        (void)fc41dRecover(WIRELESS_FC41D_DEVICE);
        return;
    }

    if (gWirelessStatus.aliveCheckStarted && !gWirelessStatus.txnStatus.isBusy) {
        if (gWirelessStatus.txnStatus.lastResult == FC41D_AT_RESULT_OK) {
            gWirelessStatus.state = eWIRELESS_STATE_ACTIVE;
        } else if (gWirelessStatus.txnStatus.lastResult != FC41D_AT_RESULT_NONE) {
            gWirelessStatus.state = eWIRELESS_STATE_FAULT;
        }
    }
}

const stWirelessStatus *wirelessGetStatus(void)
{
    return &gWirelessStatus;
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
    cfg->ble.workMode = FC41D_BLE_WORK_MODE_DISABLED;
    cfg->wifi.enableRx = true;
    cfg->wifi.rxOverwriteOnFull = true;
    cfg->wifi.workMode = FC41D_WIFI_WORK_MODE_DISABLED;
    cfg->execGuardMs = 2000U;
    cfg->bootMode = FC41D_MODE_COMMAND;
}

bool fc41dPlatformIsValidAssemble(eFc41dMapType device)
{
    (void)device;
    return huart4.Instance != NULL;
}

uint32_t fc41dPlatformGetTickMs(void)
{
    return repRtosGetTickMs();
}

eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device)
{
    (void)device;

    if (huart4.Instance == NULL) {
        return FC41D_STATUS_NOT_READY;
    }

    if (!gWirelessTransportPrepared) {
        HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_RESET);
        wirelessDelayMs(WIRELESS_RESET_ASSERT_MS);
        HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_SET);
        wirelessDelayMs(WIRELESS_RESET_RELEASE_MS);
        gWirelessTransportPrepared = true;
    }

    __HAL_UART_CLEAR_IDLEFLAG(&huart4);
    __HAL_UART_CLEAR_OREFLAG(&huart4);
    return FC41D_STATUS_OK;
}

void fc41dPlatformPollRx(eFc41dMapType device)
{
    uint8_t data;

    (void)device;
    if (huart4.Instance == NULL) {
        return;
    }

    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) != RESET) {
        __HAL_UART_CLEAR_OREFLAG(&huart4);
    }

    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) != RESET) {
        data = (uint8_t)(huart4.Instance->DR & 0x00FFU);
        if (ringBufferWrite(&gWirelessAtRxRb, &data, 1U) != 1U) {
            break;
        }
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
    stFlowParserStreamCfg cfg;
    static const char *const urcPatterns[] = {
        "+BLE*",
        "+WIFI*",
        "+STA*",
        "+AP*",
    };

    (void)device;
    if (stream == NULL) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    if (ringBufferInit(&gWirelessAtRxRb, gWirelessAtRxStorage, sizeof(gWirelessAtRxStorage)) != RINGBUFFER_OK) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.tokCfg.ringBuf = &gWirelessAtRxRb;
    cfg.tokCfg.lineBuf = gWirelessAtLineBuf;
    cfg.tokCfg.lineBufSize = sizeof(gWirelessAtLineBuf);
    cfg.cmdBuf = gWirelessAtCmdBuf;
    cfg.cmdBufSize = sizeof(gWirelessAtCmdBuf);
    cfg.payloadBuf = gWirelessAtPayloadBuf;
    cfg.payloadBufSize = sizeof(gWirelessAtPayloadBuf);
    cfg.send = wirelessFc41dSend;
    cfg.portUserCtx = &huart4;
    cfg.getTick = fc41dPlatformGetTickMs;
    cfg.urcPatterns = urcPatterns;
    cfg.urcPatternCnt = (uint8_t)(sizeof(urcPatterns) / sizeof(urcPatterns[0]));
    cfg.isUrc = NULL;
    cfg.urcMatchUserCtx = NULL;
    cfg.urcHandler = urcHandler;
    cfg.urcUserCtx = urcUserCtx;
    cfg.procBudget = 8U;
    return flowparserStreamInit(stream, &cfg);
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

    if (((lineLen >= 5U) && (memcmp(lineBuf, "+WIFI", 5U) == 0)) ||
        ((lineLen >= 4U) && (memcmp(lineBuf, "+STA", 4U) == 0)) ||
        ((lineLen >= 3U) && (memcmp(lineBuf, "+AP", 3U) == 0))) {
        *channel = FC41D_RX_CHANNEL_WIFI;
        return wirelessRouteLinePayload(lineBuf, lineLen, payloadBuf, payloadLen);
    }

    return false;
}

/**************************End of file********************************/
