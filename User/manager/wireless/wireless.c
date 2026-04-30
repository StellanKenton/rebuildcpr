/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager.
* @details  : Provides mutually exclusive BLE/WiFi service, WiFi URC handling,
*             MQTT login, and net-directory storage defaults.
**********************************************************************************/
#include "wireless.h"
#include "wireless_ble.h"
#include "wireless_internal.h"
#include "wireless_wifi.h"

#include <stdio.h>
#include <string.h>

#include "drvuart.h"
#include "../iotmanager/cprsensor_protocol.h"
#include "../iotmanager/iotmanager.h"
#include "../iotmanager/protcolmgr.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/md5/md5.h"
#include "../memory/memory.h"
#include "../../port/drvuart_port.h"

eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
eWirelessWifiState gWirelessWifiState = WIRELESS_WIFI_IDLE;
eWirelessIotState gWirelessIotState = WIRELESS_IOT_IDLE;
bool gWirelessConfigured = false;
bool gWirelessStarted = false;
bool gWirelessBleEnabled = true;
bool gWirelessWifiEnabled = false;
bool gWirelessMqttEnabled = false;
bool gWirelessWifiConfigValid = false;
bool gWirelessWifiConnected = false;
bool gWirelessWifiGotIp = false;
bool gWirelessWifiJoinPending = false;
bool gWirelessIotStorageLoaded = false;
bool gWirelessIotKeyReady = false;
bool gWirelessIotHttpPending = false;
bool gWirelessIotMqttUserCfgPending = false;
bool gWirelessIotMqttConnPending = false;
bool gWirelessIotMqttQueryPending = false;
bool gWirelessIotMqttSubPending = false;
bool gWirelessIotMqttUserCfgReady = false;
bool gWirelessIotMqttReady = false;
bool gWirelessIotMqttSubReady = false;
bool gWirelessIotMqttConnectedUrcSeen = false;
bool gWirelessRoleStartPending = false;
bool gWirelessWifiPrioritySwitchPending = false;
bool gWirelessWifiPriorityDisconnectPending = false;
bool gWirelessCipherReady = false;
bool gWirelessProtocolHandshakeDone = false;
uint8_t gWirelessIotMqttState = 0U;
uint8_t gWirelessIotMqttCfgStep = 0U;
uint8_t gWirelessIotHttpStep = 0U;
uint32_t gWirelessLastWarnTick = 0U;
uint32_t gWirelessIotNextRetryTick = 0U;
uint32_t gWirelessRoleStartTick = 0U;
uint32_t gWirelessWifiPrioritySwitchTick = 0U;
uint32_t gWirelessWifiPriorityDisconnectDeadline = 0U;
eWirelessMode gWirelessMode = WIRELESS_MODE_BLE;
eWirelessMode gWirelessTargetMode = WIRELESS_MODE_BLE;
char gWirelessWifiSsid[WIRELESS_WIFI_SSID_MAX_LEN + 1U];
char gWirelessWifiPassword[WIRELESS_WIFI_PASSWORD_MAX_LEN + 1U];
char gWirelessIotSn[WIRELESS_IOT_SN_MAX_LEN + 1U];
char gWirelessIotKey[WIRELESS_IOT_KEY_MAX_LEN + 1U];
char gWirelessIotHttpUrl[WIRELESS_IOT_URL_MAX_LEN + 1U];
char gWirelessIotMqttHost[WIRELESS_IOT_HOST_MAX_LEN + 1U];
char gWirelessIotMqttTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
char gWirelessIotMqttSubTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
char gWirelessCommandText[WIRELESS_IOT_CMD_MAX_LEN];
char gWirelessMqttUsername[64];
char gWirelessMqttPassword[WIRELESS_IOT_MD5_HEX_LEN];
char gWirelessIotHttpPayload[WIRELESS_IOT_HTTP_PAYLOAD_LEN];
char gWirelessIotHttpResponse[WIRELESS_IOT_HTTP_RESPONSE_LEN + 1U];
uint16_t gWirelessIotHttpResponseLen = 0U;
uint16_t gWirelessIotMqttPort = 1883U;
uint8_t gWirelessMqttTxBuffer[WIRELESS_TX_BUFFER_SIZE];
uint16_t gWirelessMqttTxLen = 0U;
uint8_t gWirelessWifiRxBuffer[WIRELESS_TX_BUFFER_SIZE];
uint8_t gWirelessProtocolRxBuffer[WIRELESS_TX_BUFFER_SIZE];
uint8_t gWirelessAesKey[WIRELESS_AES_KEY_SIZE];
uint16_t gWirelessWifiRxHead = 0U;
uint16_t gWirelessWifiRxTail = 0U;
uint16_t gWirelessWifiRxUsed = 0U;

static void wirelessFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg);
static void wirelessFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg);
static bool wirelessAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode);
static bool wirelessAesEncrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool wirelessAesDecrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool wirelessParseMacString(const char *text, uint8_t *mac, uint16_t macSize);
static bool wirelessTryInitCipherKey(void);
static bool wirelessParseIncomingFrame(const uint8_t *buffer,
                                       uint16_t length,
                                       stCprsensorProtocolFrameView *frameView,
                                       uint8_t *payloadBuffer,
                                       uint16_t payloadBufferSize);
static bool wirelessValidateHandshakePayload(const stCprsensorProtocolFrameView *frameView);
static bool wirelessForwardProtocolFrame(const uint8_t *buffer, uint16_t length);
static eFc41dRawMatchSta wirelessProtocolRawMatcher(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen);
static bool wirelessConfigureIfNeeded(void);
static bool wirelessStartTargetMode(void);
static void wirelessServiceProtocol(void);
static void wirelessUpdateProtocolLinks(void);
static void wirelessUpdateState(void);

bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text)
{
    uint16_t length;

    if ((buffer == NULL) || (bufferSize == 0U) || (text == NULL)) {
        return false;
    }

    length = (uint16_t)strlen(text);
    if (length >= bufferSize) {
        return false;
    }

    (void)memcpy(buffer, text, length + 1U);
    return true;
}

bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen)
{
    if ((buffer == NULL) || (bufferSize == 0U) || (text == NULL) || (textLen >= bufferSize)) {
        return false;
    }

    if (textLen > 0U) {
        (void)memcpy(buffer, text, textLen);
    }
    buffer[textLen] = '\0';
    return true;
}

void wirelessTrimText(char *text)
{
    uint16_t length;

    if (text == NULL) {
        return;
    }

    while ((*text == ' ') || (*text == '\t') || (*text == '\r') || (*text == '\n')) {
        (void)memmove(text, &text[1], strlen(text));
    }

    length = (uint16_t)strlen(text);
    while ((length > 0U) &&
           ((text[length - 1U] == ' ') || (text[length - 1U] == '\t') ||
            (text[length - 1U] == '\r') || (text[length - 1U] == '\n'))) {
        text[--length] = '\0';
    }
}

bool wirelessIsValidSnText(const char *text)
{
    uint16_t length;
    char ch;

    if (text == NULL) {
        return false;
    }

    length = (uint16_t)strlen(text);
    if ((length < (uint16_t)strlen(WIRELESS_IOT_DEFAULT_SN)) || (length > WIRELESS_IOT_SN_MAX_LEN)) {
        return false;
    }

    while (*text != '\0') {
        ch = *text++;
        if (!(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')))) {
            return false;
        }
    }

    return true;
}

bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize)
{
    uint32_t actualSize;

    if ((path == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return false;
    }

    buffer[0] = '\0';
    actualSize = 0U;
    if (!memoryReadFile(path, buffer, (uint32_t)(bufferSize - 1U), &actualSize)) {
        return false;
    }
    if (actualSize >= bufferSize) {
        return false;
    }
    buffer[actualSize] = '\0';
    wirelessTrimText(buffer);
    return buffer[0] != '\0';
}

bool wirelessWriteTextFile(const char *path, const char *text)
{
    if ((path == NULL) || (text == NULL) || (text[0] == '\0')) {
        return false;
    }
    return memoryWriteFile(path, text, (uint32_t)strlen(text));
}

bool wirelessTryParseU16Text(const char *text, uint16_t *value)
{
    uint32_t parsed = 0U;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
        return false;
    }

    while (*text != '\0') {
        if ((*text < '0') || (*text > '9')) {
            return false;
        }
        parsed = (parsed * 10U) + (uint32_t)(*text - '0');
        if (parsed > 65535UL) {
            return false;
        }
        text++;
    }

    *value = (uint16_t)parsed;
    return true;
}

static void wirelessFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
    if (codecCfg == NULL) {
        return;
    }

    (void)memset(codecCfg, 0, sizeof(*codecCfg));
    codecCfg->crc.polynomial = 0x1021U;
    codecCfg->crc.initValue = 0xFFFFU;
    codecCfg->crc.xorOut = 0U;
    codecCfg->crc.reflectInput = false;
    codecCfg->crc.reflectOutput = false;
    codecCfg->cipher.enabled = false;
    codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
}

static void wirelessFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
    wirelessFillRawCodec(codecCfg);
    if (codecCfg == NULL) {
        return;
    }

    codecCfg->cipher.enabled = gWirelessCipherReady;
    codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
    codecCfg->cipher.encrypt = wirelessAesEncrypt;
    codecCfg->cipher.decrypt = wirelessAesDecrypt;
    codecCfg->cipher.userData = gWirelessAesKey;
}

static bool wirelessAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode)
{
    stAesContext context;

    if ((userData == NULL) || (buffer == NULL) || (length == 0U) ||
        ((length % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
        return false;
    }

    if (aesInit(&context, AES_TYPE_128, AES_MODE_ECB, (const uint8_t *)userData, NULL) != AES_STATUS_OK) {
        return false;
    }

    if (encryptMode) {
        return aesEncrypt(&context, buffer, buffer, (uint32_t)length) == AES_STATUS_OK;
    }
    return aesDecrypt(&context, buffer, buffer, (uint32_t)length) == AES_STATUS_OK;
}

static bool wirelessAesEncrypt(void *userData, uint8_t *buffer, uint16_t length)
{
    return wirelessAesTransform(userData, buffer, length, true);
}

static bool wirelessAesDecrypt(void *userData, uint8_t *buffer, uint16_t length)
{
    return wirelessAesTransform(userData, buffer, length, false);
}

static bool wirelessParseMacString(const char *text, uint8_t *mac, uint16_t macSize)
{
    uint16_t index;
    uint8_t highNibble;
    uint8_t value;
    char ch;
    bool highReady;

    if ((text == NULL) || (mac == NULL) || (macSize < CPRSENSOR_PROTOCOL_MAC_LEN)) {
        return false;
    }

    index = 0U;
    highNibble = 0U;
    highReady = false;
    while (*text != '\0') {
        ch = *text++;
        if ((ch == ':') || (ch == '-') || (ch == ' ')) {
            continue;
        }

        if ((ch >= '0') && (ch <= '9')) {
            value = (uint8_t)(ch - '0');
        } else if ((ch >= 'a') && (ch <= 'f')) {
            value = (uint8_t)(ch - 'a' + 10);
        } else if ((ch >= 'A') && (ch <= 'F')) {
            value = (uint8_t)(ch - 'A' + 10);
        } else {
            return false;
        }

        if (!highReady) {
            highNibble = (uint8_t)(value << 4U);
            highReady = true;
        } else {
            if (index >= CPRSENSOR_PROTOCOL_MAC_LEN) {
                return false;
            }
            mac[index++] = (uint8_t)(highNibble | value);
            highReady = false;
        }
    }

    return (!highReady) && (index == CPRSENSOR_PROTOCOL_MAC_LEN);
}

static bool wirelessTryInitCipherKey(void)
{
    char macText[32];
    uint8_t macBytes[CPRSENSOR_PROTOCOL_MAC_LEN];

    if (gWirelessCipherReady) {
        return true;
    }

    if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) ||
        !wirelessParseMacString(macText, macBytes, (uint16_t)sizeof(macBytes)) ||
        (md5CalcData(macBytes, (uint32_t)sizeof(macBytes), gWirelessAesKey) != MD5_STATUS_OK)) {
        return false;
    }

    gWirelessCipherReady = true;
    (void)protcolMgrTryInitCipherKey();
    return true;
}

static bool wirelessParseIncomingFrame(const uint8_t *buffer,
                                       uint16_t length,
                                       stCprsensorProtocolFrameView *frameView,
                                       uint8_t *payloadBuffer,
                                       uint16_t payloadBufferSize)
{
    stCprsensorProtocolCodecCfg rawCodec;
    stCprsensorProtocolCodecCfg cipherCodec;
    stCprsensorProtocolFrameView cipherView;

    if ((buffer == NULL) || (frameView == NULL) || (payloadBuffer == NULL) || (payloadBufferSize == 0U)) {
        return false;
    }

    wirelessFillRawCodec(&rawCodec);
    if (cprsensorProtocolParseFrame(buffer,
                                    length,
                                    &rawCodec,
                                    payloadBuffer,
                                    payloadBufferSize,
                                    frameView) != CPRSENSOR_PROTOCOL_STATUS_OK) {
        return false;
    }

    if (!wirelessTryInitCipherKey()) {
        return frameView->cmd == CPRSENSOR_PROTOCOL_CMD_HANDSHAKE;
    }

    if ((frameView->encodedPayloadLen == 0U) ||
        ((frameView->encodedPayloadLen % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
        return true;
    }

    wirelessFillCipherCodec(&cipherCodec);
    if (cprsensorProtocolParseFrame(buffer,
                                    length,
                                    &cipherCodec,
                                    payloadBuffer,
                                    payloadBufferSize,
                                    &cipherView) == CPRSENSOR_PROTOCOL_STATUS_OK) {
        *frameView = cipherView;
    }

    return true;
}

static bool wirelessValidateHandshakePayload(const stCprsensorProtocolFrameView *frameView)
{
    char macText[32];
    uint8_t macBytes[CPRSENSOR_PROTOCOL_MAC_LEN];
    uint16_t index;

    if ((frameView == NULL) || (frameView->cmd != CPRSENSOR_PROTOCOL_CMD_HANDSHAKE) ||
        (frameView->payload == NULL) ||
        (frameView->encodedPayloadLen != CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) ||
        (frameView->payloadLen != CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE)) {
        return false;
    }

    if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) ||
        !wirelessParseMacString(macText, macBytes, (uint16_t)sizeof(macBytes))) {
        return false;
    }

    if (memcmp(frameView->payload, macBytes, CPRSENSOR_PROTOCOL_MAC_LEN) != 0) {
        return false;
    }

    for (index = CPRSENSOR_PROTOCOL_MAC_LEN; index < frameView->payloadLen; ++index) {
        if (frameView->payload[index] != 0U) {
            return false;
        }
    }

    return true;
}

static bool wirelessForwardProtocolFrame(const uint8_t *buffer, uint16_t length)
{
    stCprsensorProtocolFrameView frameView;
    uint8_t payloadBuffer[WIRELESS_TX_BUFFER_SIZE];

    if (!wirelessParseIncomingFrame(buffer,
                                    length,
                                    &frameView,
                                    payloadBuffer,
                                    (uint16_t)sizeof(payloadBuffer))) {
        LOG_W(WIRELESS_LOG_TAG, "ignore invalid protocol frame len=%u", (unsigned int)length);
        return false;
    }

    if (!gWirelessProtocolHandshakeDone) {
        if (frameView.cmd != CPRSENSOR_PROTOCOL_CMD_HANDSHAKE) {
            LOG_W(WIRELESS_LOG_TAG,
                  "ignore cmd=0x%02X before handshake",
                  (unsigned int)frameView.cmd);
            return false;
        }

        if (!wirelessValidateHandshakePayload(&frameView)) {
            LOG_W(WIRELESS_LOG_TAG, "ignore invalid handshake payload");
            return false;
        }

        gWirelessProtocolHandshakeDone = true;
        LOG_I(WIRELESS_LOG_TAG, "ble protocol handshake ok");
    }

    if (!protcolMgrPushReceivedData(IOT_MANAGER_LINK_BLE, buffer, length)) {
        LOG_W(WIRELESS_LOG_TAG, "ble rx forward failed len=%u", (unsigned int)length);
        return false;
    }

    return true;
}

static eFc41dRawMatchSta wirelessProtocolRawMatcher(void *userData,
                                                    const uint8_t *buf,
                                                    uint16_t availLen,
                                                    uint16_t *frameLen)
{
    eCprsensorProtocolStatus status;
    uint16_t parsedLen;

    (void)userData;
    if ((buf == NULL) || (frameLen == NULL) || (availLen == 0U)) {
        return FC41D_RAW_MATCH_NONE;
    }

    parsedLen = 0U;
    status = cprsensorProtocolTryGetFrameLength(buf, availLen, &parsedLen);
    if (status == CPRSENSOR_PROTOCOL_STATUS_OK) {
        *frameLen = parsedLen;
        return FC41D_RAW_MATCH_OK;
    }
    if ((buf[0] == CPRSENSOR_PROTOCOL_FRAME_HEAD0) && (status == CPRSENSOR_PROTOCOL_STATUS_ERROR_LENGTH)) {
        return FC41D_RAW_MATCH_NEED_MORE;
    }

    return FC41D_RAW_MATCH_NONE;
}

static bool wirelessConfigureIfNeeded(void)
{
    stFc41dCfg cfg;
    stFc41dBleCfg bleCfg;

    if (gWirelessConfigured) {
        return true;
    }

    if (drvUartInit(DRVUART_WIFI) != DRV_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        LOG_E(WIRELESS_LOG_TAG, "wifi uart init fail");
        return false;
    }
    if ((fc41dGetDefCfg(WIRELESS_FC41D_DEVICE, &cfg) != FC41D_STATUS_OK) ||
        (fc41dSetCfg(WIRELESS_FC41D_DEVICE, &cfg) != FC41D_STATUS_OK) ||
        (fc41dGetDefBleCfg(WIRELESS_FC41D_DEVICE, &bleCfg) != FC41D_STATUS_OK)) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    wirelessBleLoadDefaultConfig(&bleCfg);
    if ((fc41dSetBleCfg(WIRELESS_FC41D_DEVICE, &bleCfg) != FC41D_STATUS_OK) ||
        (fc41dInit(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) ||
        (fc41dSetUrcMatcher(WIRELESS_FC41D_DEVICE, wirelessFc41dUrcMatcher, NULL) != FC41D_STATUS_OK) ||
        (fc41dSetUrcHandler(WIRELESS_FC41D_DEVICE, wirelessFc41dUrcHandler, NULL) != FC41D_STATUS_OK) ||
        (fc41dSetRawMatcher(WIRELESS_FC41D_DEVICE, wirelessProtocolRawMatcher, NULL) != FC41D_STATUS_OK)) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    gWirelessConfigured = true;
    return true;
}

static bool wirelessStartTargetMode(void)
{
    eFc41dRole role;
    uint32_t nowTick;

    if (!gWirelessConfigured) {
        return false;
    }

    if (gWirelessStarted && (gWirelessMode == gWirelessTargetMode)) {
        return true;
    }

    if (gWirelessStarted) {
        fc41dStop(WIRELESS_FC41D_DEVICE);
        gWirelessStarted = false;
        gWirelessRoleStartPending = true;
        gWirelessRoleStartTick = repRtosGetTickMs() + WIRELESS_ROLE_SWITCH_SETTLE_MS;
        gWirelessWifiConnected = false;
        gWirelessWifiGotIp = false;
        gWirelessWifiJoinPending = false;
        wirelessResetIotRuntime();
        gWirelessWifiState = WIRELESS_WIFI_IDLE;
        LOG_I(WIRELESS_LOG_TAG, "fc41d stop for mode switch target=%s",
              (gWirelessTargetMode == WIRELESS_MODE_WIFI) ? "wifi" : "ble");
        return true;
    }

    if (gWirelessRoleStartPending) {
        nowTick = repRtosGetTickMs();
        if ((int32_t)(nowTick - gWirelessRoleStartTick) < 0) {
            return true;
        }
        gWirelessRoleStartPending = false;
    }

    role = (gWirelessTargetMode == WIRELESS_MODE_WIFI) ? FC41D_ROLE_WIFI_STATION : FC41D_ROLE_BLE_PERIPHERAL;
    if (fc41dStart(WIRELESS_FC41D_DEVICE, role) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    gWirelessStarted = true;
    gWirelessMode = gWirelessTargetMode;
    gWirelessWifiConnected = false;
    gWirelessWifiGotIp = false;
    gWirelessWifiJoinPending = false;
    wirelessResetIotRuntime();
    if (gWirelessMode == WIRELESS_MODE_BLE) {
        gWirelessWifiState = WIRELESS_WIFI_IDLE;
    } else if (gWirelessWifiEnabled && gWirelessWifiConfigValid) {
        gWirelessWifiState = WIRELESS_WIFI_INITIALIZING;
    }
    LOG_I(WIRELESS_LOG_TAG, "fc41d start mode=%s", (gWirelessMode == WIRELESS_MODE_WIFI) ? "wifi" : "ble");
    return true;
}

static void wirelessServiceProtocol(void)
{
    uint16_t length;

    if (gWirelessMode == WIRELESS_MODE_BLE) {
        length = wirelessReadBleData(gWirelessProtocolRxBuffer, (uint16_t)sizeof(gWirelessProtocolRxBuffer));
        if (length > 0U) {
            (void)wirelessForwardProtocolFrame(gWirelessProtocolRxBuffer, length);
        }
    }

    iotManagerProcess();
}

static void wirelessUpdateProtocolLinks(void)
{
    stIotManagerLinkRuntime runtime;
    const stFc41dState *state;

    state = fc41dGetState(WIRELESS_FC41D_DEVICE);
    (void)memset(&runtime, 0, sizeof(runtime));
    runtime.linkId = IOT_MANAGER_LINK_BLE;
    runtime.installed = true;
    runtime.enabled = gWirelessBleEnabled || (gWirelessMode == WIRELESS_MODE_BLE);
    runtime.moduleReady = (state != NULL) && state->isReady && (gWirelessMode == WIRELESS_MODE_BLE);
    if ((state == NULL) || !state->isBleConnected || (gWirelessMode != WIRELESS_MODE_BLE)) {
        gWirelessProtocolHandshakeDone = false;
    }
    runtime.peerConnected = (state != NULL) && state->isBleConnected && gWirelessProtocolHandshakeDone;
    runtime.state = !runtime.enabled ? IOT_MANAGER_LINK_STATE_DISABLED :
        (runtime.peerConnected ? IOT_MANAGER_LINK_STATE_SERVICE_READY :
         (runtime.moduleReady ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_INITING));
    runtime.caps.supportBleLocal = true;
    (void)iotManagerUpdateLinkState(IOT_MANAGER_LINK_BLE, &runtime);

    (void)memset(&runtime, 0, sizeof(runtime));
    runtime.linkId = IOT_MANAGER_LINK_WIFI;
    runtime.installed = true;
    runtime.enabled = gWirelessWifiEnabled;
    runtime.moduleReady = (state != NULL) && state->isReady && (gWirelessMode == WIRELESS_MODE_WIFI);
    runtime.netReady = gWirelessWifiGotIp;
    runtime.mqttAuthReady = gWirelessIotKeyReady;
    runtime.mqttReady = gWirelessIotMqttReady;
    runtime.state = !runtime.enabled ? IOT_MANAGER_LINK_STATE_DISABLED :
        (runtime.mqttReady ? IOT_MANAGER_LINK_STATE_SERVICE_READY :
         (runtime.netReady ? IOT_MANAGER_LINK_STATE_NET_READY :
          (runtime.moduleReady ? IOT_MANAGER_LINK_STATE_NET_CONNECTING : IOT_MANAGER_LINK_STATE_INITING)));
    runtime.caps.supportMqttAuthHttp = true;
    runtime.caps.supportMqtt = true;
    (void)iotManagerUpdateLinkState(IOT_MANAGER_LINK_WIFI, &runtime);
}

eWirelessMode wirelessResolveTargetMode(void)
{
    if (gWirelessBleEnabled) {
        return WIRELESS_MODE_BLE;
    }

    if (gWirelessWifiEnabled && gWirelessWifiConfigValid) {
        return WIRELESS_MODE_WIFI;
    }

    return WIRELESS_MODE_BLE;
}

static void wirelessUpdateState(void)
{
    const stFc41dState *state;

    state = fc41dGetState(WIRELESS_FC41D_DEVICE);
    if (state == NULL) {
        gWirelessState = gWirelessConfigured ? eWIRELESS_STATE_ERROR : eWIRELESS_STATE_INIT;
        return;
    }

    if (state->runState == FC41D_RUN_ERROR) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        gWirelessWifiState = WIRELESS_WIFI_ERROR;
        return;
    }
    gWirelessState = state->isReady ? eWIRELESS_STATE_NORMAL : eWIRELESS_STATE_INIT;
    if ((gWirelessMode == WIRELESS_MODE_WIFI) && state->isReady && gWirelessWifiEnabled && !gWirelessWifiGotIp &&
        (gWirelessWifiState == WIRELESS_WIFI_IDLE)) {
        gWirelessWifiState = WIRELESS_WIFI_READY;
    }
}

bool wirelessInit(void)
{
    (void)wirelessLoadStorageConfig();
    if (!wirelessConfigureIfNeeded()) {
        return false;
    }
    gWirelessTargetMode = wirelessResolveTargetMode();
    if (!wirelessStartTargetMode()) {
        return false;
    }
    wirelessUpdateState();
    return gWirelessState != eWIRELESS_STATE_ERROR;
}

void wirelessProcess(void)
{
    eFc41dStatus status;
    uint32_t nowTickMs;

    (void)wirelessLoadStorageConfig();
    if (!wirelessConfigureIfNeeded()) {
        return;
    }
    gWirelessTargetMode = wirelessResolveTargetMode();
    if (!wirelessStartTargetMode()) {
        return;
    }

    nowTickMs = repRtosGetTickMs();
    status = fc41dProcess(WIRELESS_FC41D_DEVICE, nowTickMs);
    if ((status != FC41D_STATUS_OK) && ((nowTickMs - gWirelessLastWarnTick) >= WIRELESS_RETRY_LOG_MS)) {
        gWirelessLastWarnTick = nowTickMs;
        LOG_W(WIRELESS_LOG_TAG, "fc41d process fail status=%d", (int)status);
    }

    wirelessUpdateState();
    wirelessServiceWifi();
    wirelessServiceIot();
    wirelessUpdateProtocolLinks();
    wirelessServiceProtocol();
    wirelessServicePrioritySwitch();
}

const eWirelessState *wirelessGetStatus(void)
{
    return &gWirelessState;
}

/**************************End of file********************************/
