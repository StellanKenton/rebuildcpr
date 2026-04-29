/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager.
* @details  : Provides mutually exclusive BLE/WiFi service, WiFi URC handling,
*             MQTT login, and net-directory storage defaults.
**********************************************************************************/
#include "wireless.h"
#include "wireless_ble.h"
#include "wireless_wifi.h"

#include <stdio.h>
#include <string.h>

#include "drvuart.h"
#include "../iotmanager/cprsensor_protocol.h"
#include "../iotmanager/iotmanager.h"
#include "../iotmanager/protcolmgr.h"
#include "../../../rep/module/fc41d/fc41d_http.h"
#include "../../../rep/module/fc41d/fc41d_mqtt.h"
#include "../../../rep/module/fc41d/fc41d_wifi.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/jsonparser/jsonparser.h"
#include "../../../rep/tools/md5/md5.h"
#include "../memory/memory.h"
#include "../../port/drvuart_port.h"

#define WIRELESS_RETRY_LOG_MS            1000U
#define WIRELESS_TX_BUFFER_SIZE          640U
#define WIRELESS_IOT_MD5_HEX_LEN         33U
#define WIRELESS_MODE_SWITCH_DELAY_MS    500U
#define WIRELESS_BLE_DISCONNECT_WAIT_MS  2000U
#define WIRELESS_ROLE_SWITCH_SETTLE_MS   500U

static eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
static eWirelessWifiState gWirelessWifiState = WIRELESS_WIFI_IDLE;
static eWirelessIotState gWirelessIotState = WIRELESS_IOT_IDLE;
static bool gWirelessConfigured = false;
static bool gWirelessStarted = false;
static bool gWirelessBleEnabled = true;
static bool gWirelessWifiEnabled = false;
static bool gWirelessMqttEnabled = false;
static bool gWirelessWifiConfigValid = false;
static bool gWirelessWifiConnected = false;
static bool gWirelessWifiGotIp = false;
static bool gWirelessWifiJoinPending = false;
static bool gWirelessIotStorageLoaded = false;
static bool gWirelessIotKeyReady = false;
static bool gWirelessIotHttpPending = false;
static bool gWirelessIotMqttUserCfgPending = false;
static bool gWirelessIotMqttConnPending = false;
static bool gWirelessIotMqttQueryPending = false;
static bool gWirelessIotMqttSubPending = false;
static bool gWirelessIotMqttUserCfgReady = false;
static bool gWirelessIotMqttReady = false;
static bool gWirelessIotMqttSubReady = false;
static bool gWirelessIotMqttConnectedUrcSeen = false;
static bool gWirelessRoleStartPending = false;
static bool gWirelessWifiPrioritySwitchPending = false;
static bool gWirelessWifiPriorityDisconnectPending = false;
static uint8_t gWirelessIotMqttState = 0U;
static uint8_t gWirelessIotMqttCfgStep = 0U;
static uint8_t gWirelessIotHttpStep = 0U;
static uint32_t gWirelessLastWarnTick = 0U;
static uint32_t gWirelessIotNextRetryTick = 0U;
static uint32_t gWirelessRoleStartTick = 0U;
static uint32_t gWirelessWifiPrioritySwitchTick = 0U;
static uint32_t gWirelessWifiPriorityDisconnectDeadline = 0U;
static eWirelessMode gWirelessMode = WIRELESS_MODE_BLE;
static eWirelessMode gWirelessTargetMode = WIRELESS_MODE_BLE;
static char gWirelessWifiSsid[WIRELESS_WIFI_SSID_MAX_LEN + 1U];
static char gWirelessWifiPassword[WIRELESS_WIFI_PASSWORD_MAX_LEN + 1U];
static char gWirelessIotSn[WIRELESS_IOT_SN_MAX_LEN + 1U];
static char gWirelessIotKey[WIRELESS_IOT_KEY_MAX_LEN + 1U];
static char gWirelessIotHttpUrl[WIRELESS_IOT_URL_MAX_LEN + 1U];
static char gWirelessIotMqttHost[WIRELESS_IOT_HOST_MAX_LEN + 1U];
static char gWirelessIotMqttTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
static char gWirelessIotMqttSubTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
static char gWirelessCommandText[WIRELESS_IOT_CMD_MAX_LEN];
static char gWirelessMqttUsername[64];
static char gWirelessMqttPassword[WIRELESS_IOT_MD5_HEX_LEN];
static char gWirelessIotHttpPayload[WIRELESS_IOT_HTTP_PAYLOAD_LEN];
static char gWirelessIotHttpResponse[WIRELESS_IOT_HTTP_RESPONSE_LEN + 1U];
static uint16_t gWirelessIotHttpResponseLen = 0U;
static uint16_t gWirelessIotMqttPort = 1883U;
static uint8_t gWirelessMqttTxBuffer[WIRELESS_TX_BUFFER_SIZE];
static uint16_t gWirelessMqttTxLen = 0U;
static uint8_t gWirelessWifiRxBuffer[WIRELESS_TX_BUFFER_SIZE];
static uint8_t gWirelessProtocolRxBuffer[WIRELESS_TX_BUFFER_SIZE];
static uint16_t gWirelessWifiRxHead = 0U;
static uint16_t gWirelessWifiRxTail = 0U;
static uint16_t gWirelessWifiRxUsed = 0U;

static bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text);
static bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen);
static void wirelessTrimText(char *text);
static bool wirelessIsValidSnText(const char *text);
static bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize);
static bool wirelessWriteTextFile(const char *path, const char *text);
static bool wirelessTryParseU16Text(const char *text, uint16_t *value);
static void wirelessFillDefaultStorageConfig(void);
static bool wirelessLoadStorageConfig(void);
static bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text);
static bool wirelessCopyMacCompact(char *buffer, uint16_t bufferSize, const char *macText);
static bool wirelessBuildHttpAuthPayload(char *buffer, uint16_t bufferSize);
static bool wirelessBuildMqttLogin(char *username, uint16_t usernameSize, char *password, uint16_t passwordSize);
static bool wirelessTryParseHttpKey(void);
static void wirelessWifiJoinLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessIotHttpLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessFc41dUrcMatcher(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessFc41dUrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static eFc41dRawMatchSta wirelessProtocolRawMatcher(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen);
static bool wirelessConfigureIfNeeded(void);
static bool wirelessStartTargetMode(void);
static void wirelessResetIotRuntime(void);
static void wirelessStoreWifiRx(const uint8_t *buffer, uint16_t length);
static bool wirelessSubmitSimpleCommand(const char *cmdText);
static void wirelessServiceWifi(void);
static void wirelessServiceIot(void);
static void wirelessServiceProtocol(void);
static void wirelessUpdateProtocolLinks(void);
static void wirelessServicePrioritySwitch(void);
static eWirelessMode wirelessResolveTargetMode(void);
static void wirelessUpdateState(void);

static bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text)
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

static bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen)
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

static void wirelessTrimText(char *text)
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

static bool wirelessIsValidSnText(const char *text)
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

static bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize)
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

static bool wirelessWriteTextFile(const char *path, const char *text)
{
    if ((path == NULL) || (text == NULL) || (text[0] == '\0')) {
        return false;
    }
    return memoryWriteFile(path, text, (uint32_t)strlen(text));
}

static bool wirelessTryParseU16Text(const char *text, uint16_t *value)
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

static void wirelessFillDefaultStorageConfig(void)
{
    (void)memoryMkdir(WIRELESS_NET_DIR_PATH);

    if (gWirelessWifiSsid[0] == '\0') {
        (void)wirelessCopyText(gWirelessWifiSsid, (uint16_t)sizeof(gWirelessWifiSsid), wirelessWifiGetDefaultSsid());
        (void)wirelessWriteTextFile(WIRELESS_WIFI_SSID_PATH, gWirelessWifiSsid);
    }
    if (gWirelessWifiPassword[0] == '\0') {
        (void)wirelessCopyText(gWirelessWifiPassword,
                               (uint16_t)sizeof(gWirelessWifiPassword),
                               wirelessWifiGetDefaultPassword());
        (void)wirelessWriteTextFile(WIRELESS_WIFI_PASSWORD_PATH, gWirelessWifiPassword);
    }
    if (!wirelessIsValidSnText(gWirelessIotSn)) {
        (void)wirelessCopyText(gWirelessIotSn, (uint16_t)sizeof(gWirelessIotSn), wirelessWifiGetDefaultSn());
        (void)wirelessWriteTextFile(WIRELESS_IOT_SN_PATH, gWirelessIotSn);
    }
    if (gWirelessIotHttpUrl[0] == '\0') {
        (void)wirelessCopyText(gWirelessIotHttpUrl, (uint16_t)sizeof(gWirelessIotHttpUrl), wirelessWifiGetDefaultHttpUrl());
        (void)wirelessWriteTextFile(WIRELESS_IOT_HTTP_URL_PATH, gWirelessIotHttpUrl);
    }
    if (gWirelessIotMqttHost[0] == '\0') {
        (void)wirelessCopyText(gWirelessIotMqttHost, (uint16_t)sizeof(gWirelessIotMqttHost), wirelessWifiGetDefaultMqttHost());
        (void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_HOST_PATH, gWirelessIotMqttHost);
    }
    if (!memoryExists(WIRELESS_IOT_MQTT_PORT_PATH)) {
        (void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_PORT_PATH, wirelessWifiGetDefaultMqttPortText());
    }
}

static bool wirelessLoadStorageConfig(void)
{
    char portText[8];

    if (gWirelessIotStorageLoaded) {
        return true;
    }
    if (!memoryIsReady() && !memoryInit()) {
        return false;
    }

    (void)wirelessReadTextFile(WIRELESS_WIFI_SSID_PATH, gWirelessWifiSsid, (uint16_t)sizeof(gWirelessWifiSsid));
    (void)wirelessReadTextFile(WIRELESS_WIFI_PASSWORD_PATH, gWirelessWifiPassword, (uint16_t)sizeof(gWirelessWifiPassword));
    (void)wirelessReadTextFile(WIRELESS_IOT_SN_PATH, gWirelessIotSn, (uint16_t)sizeof(gWirelessIotSn));
    (void)wirelessReadTextFile(WIRELESS_IOT_HTTP_URL_PATH, gWirelessIotHttpUrl, (uint16_t)sizeof(gWirelessIotHttpUrl));
    (void)wirelessReadTextFile(WIRELESS_IOT_MQTT_KEY_PATH, gWirelessIotKey, (uint16_t)sizeof(gWirelessIotKey));
    (void)wirelessReadTextFile(WIRELESS_IOT_MQTT_HOST_PATH, gWirelessIotMqttHost, (uint16_t)sizeof(gWirelessIotMqttHost));
    (void)wirelessReadTextFile(WIRELESS_IOT_MQTT_TOPIC_PATH, gWirelessIotMqttTopic, (uint16_t)sizeof(gWirelessIotMqttTopic));
    (void)wirelessReadTextFile(WIRELESS_IOT_MQTT_SUB_TOPIC_PATH, gWirelessIotMqttSubTopic, (uint16_t)sizeof(gWirelessIotMqttSubTopic));
    if (wirelessReadTextFile(WIRELESS_IOT_MQTT_PORT_PATH, portText, (uint16_t)sizeof(portText))) {
        (void)wirelessTryParseU16Text(portText, &gWirelessIotMqttPort);
    }

    wirelessFillDefaultStorageConfig();
    gWirelessWifiConfigValid = gWirelessWifiSsid[0] != '\0';
    gWirelessBleEnabled = wirelessBleDefaultEnabled();
    gWirelessWifiEnabled = wirelessWifiDefaultEnabled();
    gWirelessMqttEnabled = wirelessMqttDefaultEnabled();
    gWirelessIotKeyReady = gWirelessIotKey[0] != '\0';
    if (gWirelessIotMqttTopic[0] == '\0') {
        (void)snprintf(gWirelessIotMqttTopic, sizeof(gWirelessIotMqttTopic), "CPR/%s/event/transfer", gWirelessIotSn);
    }
    if (gWirelessIotMqttSubTopic[0] == '\0') {
        (void)snprintf(gWirelessIotMqttSubTopic, sizeof(gWirelessIotMqttSubTopic), "CPR/%s/cmd/transfer", gWirelessIotSn);
    }

    gWirelessIotStorageLoaded = true;
    LOG_I(WIRELESS_LOG_TAG,
          "net cfg sn=%s wifi=%u key=%u host=%s port=%u",
          gWirelessIotSn,
          gWirelessWifiConfigValid ? 1U : 0U,
          gWirelessIotKeyReady ? 1U : 0U,
          gWirelessIotMqttHost,
          (unsigned int)gWirelessIotMqttPort);
    return true;
}

static bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text)
{
    uint16_t textLen;

    if ((buffer == NULL) || (text == NULL)) {
        return false;
    }

    textLen = (uint16_t)strlen(text);
    return (length >= textLen) && (memcmp(buffer, text, textLen) == 0);
}

static bool wirelessCopyMacCompact(char *buffer, uint16_t bufferSize, const char *macText)
{
    uint16_t outIndex = 0U;
    char ch;

    if ((buffer == NULL) || (bufferSize < 13U) || (macText == NULL)) {
        return false;
    }

    while (*macText != '\0') {
        ch = *macText++;
        if ((ch == ':') || (ch == '-') || (ch == ' ')) {
            continue;
        }
        if ((ch >= 'a') && (ch <= 'f')) {
            ch = (char)(ch - ('a' - 'A'));
        }
        if (!(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F'))) || (outIndex >= 12U)) {
            return false;
        }
        buffer[outIndex++] = ch;
    }
    buffer[outIndex] = '\0';
    return outIndex == 12U;
}

static bool wirelessBuildHttpAuthPayload(char *buffer, uint16_t bufferSize)
{
    char macText[FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH + 1U];
    char macCompact[13];
    char signSource[128];
    char signHex[WIRELESS_IOT_MD5_HEX_LEN];
    int length;

    if ((buffer == NULL) || (bufferSize == 0U) || (gWirelessIotSn[0] == '\0')) {
        return false;
    }

    if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) ||
        !wirelessCopyMacCompact(macCompact, (uint16_t)sizeof(macCompact), macText)) {
        (void)wirelessCopyText(macCompact, (uint16_t)sizeof(macCompact), "000000000000");
    }

    length = snprintf(signSource, sizeof(signSource), "%s%s%s%s",
                      gWirelessIotSn, macCompact, WIRELESS_IOT_HTTP_RANDOM, WIRELESS_IOT_PRODUCT_SECRET);
    if ((length <= 0) || ((uint16_t)length >= sizeof(signSource)) ||
        (md5StringToHex32(signSource, signHex, 1U) != MD5_STATUS_OK)) {
        return false;
    }

    length = snprintf(buffer, bufferSize,
                      "{\"deviceId\":\"%s\",\"moduleId\":\"%s\",\"random\":\"%s\",\"sign\":\"%s\"}",
                      gWirelessIotSn, macCompact, WIRELESS_IOT_HTTP_RANDOM, signHex);
    return (length > 0) && ((uint16_t)length < bufferSize);
}

static bool wirelessBuildMqttLogin(char *username, uint16_t usernameSize, char *password, uint16_t passwordSize)
{
    char signSource[160];
    uint32_t timestampMs;
    int length;

    if ((username == NULL) || (password == NULL) || (gWirelessIotSn[0] == '\0') || (gWirelessIotKey[0] == '\0')) {
        return false;
    }

    timestampMs = repRtosGetTickMs();
    length = snprintf(username, usernameSize, "%s|%lu", gWirelessIotSn, (unsigned long)timestampMs);
    if ((length <= 0) || ((uint16_t)length >= usernameSize)) {
        return false;
    }
    length = snprintf(signSource, sizeof(signSource), "%s|%s", username, gWirelessIotKey);
    if ((length <= 0) || ((uint16_t)length >= sizeof(signSource))) {
        return false;
    }
    return md5StringToHex32(signSource, password, 0U) == MD5_STATUS_OK;
}

static bool wirelessTryParseHttpKey(void)
{
    static const char *const keyNames[] = {"result", "deviceSecret", "device_secret", "mqtt_key", "key", "token", "password", "secret"};
    uint32_t index;
    uint16_t responseLen;

    responseLen = (uint16_t)strlen(gWirelessIotHttpResponse);
    for (index = 0U; index < (uint32_t)(sizeof(keyNames) / sizeof(keyNames[0])); index++) {
        if (jsonParserFindString(gWirelessIotHttpResponse,
                                 responseLen,
                                 keyNames[index],
                                 gWirelessIotKey,
                                 (uint16_t)sizeof(gWirelessIotKey),
                                 NULL) == JSON_PARSER_STATUS_OK) {
            gWirelessIotKeyReady = true;
            (void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_KEY_PATH, gWirelessIotKey);
            LOG_I(WIRELESS_LOG_TAG, "iot key cached field=%s", keyNames[index]);
            return true;
        }
    }
    return false;
}

static void wirelessWifiJoinLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    (void)userData;
    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }
    LOG_I(WIRELESS_LOG_TAG, "wifi join rsp=%.*s", (int)lineLen, (const char *)lineBuf);
}

static void wirelessIotHttpLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    uint16_t copyLen;

    (void)userData;
    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }
    LOG_I(WIRELESS_LOG_TAG, "http rsp=%.*s", (int)lineLen, (const char *)lineBuf);
    if (wirelessMatchPrefix(lineBuf, lineLen, "OK") || wirelessMatchPrefix(lineBuf, lineLen, "ERROR") ||
        wirelessMatchPrefix(lineBuf, lineLen, "CONNECT") || wirelessMatchPrefix(lineBuf, lineLen, "+QHTTPPOST:") ||
        wirelessMatchPrefix(lineBuf, lineLen, "+QHTTPREAD:")) {
        return;
    }

    copyLen = lineLen;
    if (copyLen > (uint16_t)(WIRELESS_IOT_HTTP_RESPONSE_LEN - gWirelessIotHttpResponseLen)) {
        copyLen = (uint16_t)(WIRELESS_IOT_HTTP_RESPONSE_LEN - gWirelessIotHttpResponseLen);
    }
    if (copyLen == 0U) {
        return;
    }
    (void)memcpy(&gWirelessIotHttpResponse[gWirelessIotHttpResponseLen], lineBuf, copyLen);
    gWirelessIotHttpResponseLen = (uint16_t)(gWirelessIotHttpResponseLen + copyLen);
    gWirelessIotHttpResponse[gWirelessIotHttpResponseLen] = '\0';
}

static bool wirelessFc41dUrcMatcher(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    (void)userData;
    return fc41dWifiIsUrc(lineBuf, lineLen) || fc41dMqttIsUrc(lineBuf, lineLen);
}

static void wirelessFc41dUrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    const uint8_t *payload;
    uint16_t payloadLen;

    (void)userData;
    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI CONNECTED")) {
        gWirelessWifiConnected = true;
        gWirelessWifiState = WIRELESS_WIFI_WAITING_CONNECTION;
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI GOT IP")) {
        gWirelessWifiConnected = true;
        gWirelessWifiGotIp = true;
        gWirelessWifiJoinPending = false;
        gWirelessWifiState = WIRELESS_WIFI_CONNECTED;
        LOG_I(WIRELESS_LOG_TAG, "wifi got ip");
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "+QSTASTAT:WLAN_CONNECTED")) {
        gWirelessWifiConnected = true;
        gWirelessWifiGotIp = true;
        gWirelessWifiJoinPending = false;
        gWirelessWifiState = WIRELESS_WIFI_CONNECTED;
        LOG_I(WIRELESS_LOG_TAG, "wifi connected");
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECT") ||
        wirelessMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECTED") ||
        wirelessMatchPrefix(lineBuf, lineLen, "+CWJAP:") ||
        wirelessMatchPrefix(lineBuf, lineLen, "+QSTASTAT:WLAN_DISCONNECTED")) {
        gWirelessWifiConnected = false;
        gWirelessWifiGotIp = false;
        gWirelessWifiJoinPending = false;
        gWirelessIotMqttReady = false;
        gWirelessIotMqttSubReady = false;
        gWirelessWifiState = gWirelessWifiConfigValid ? WIRELESS_WIFI_READY : WIRELESS_WIFI_IDLE;
        return;
    }

    if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTCONNECTED:")) {
        gWirelessIotMqttConnectedUrcSeen = true;
        gWirelessIotMqttReady = true;
        gWirelessIotMqttState = 4U;
        gWirelessIotState = WIRELESS_IOT_MQTT_READY;
        LOG_I(WIRELESS_LOG_TAG, "mqtt broker connected");
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "+QMTCONN: 0,0,0")) {
        gWirelessIotMqttUserCfgReady = true;
        gWirelessIotMqttConnectedUrcSeen = true;
        gWirelessIotMqttReady = true;
        gWirelessIotMqttState = 4U;
        gWirelessIotMqttConnPending = false;
        gWirelessIotState = WIRELESS_IOT_MQTT_READY;
        LOG_I(WIRELESS_LOG_TAG, "mqtt broker connected");
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTDISCONNECTED:")) {
        gWirelessIotMqttConnectedUrcSeen = false;
        gWirelessIotMqttReady = false;
        gWirelessIotMqttSubReady = false;
        gWirelessIotMqttState = 3U;
        gWirelessIotState = gWirelessMqttEnabled ? WIRELESS_IOT_AUTH_READY : WIRELESS_IOT_IDLE;
        LOG_W(WIRELESS_LOG_TAG, "mqtt broker disconnected");
        return;
    }
    if (fc41dMqttTryParseSubRecv(lineBuf, lineLen, &payload, &payloadLen)) {
        wirelessStoreWifiRx(payload, payloadLen);
        (void)protcolMgrPushReceivedData(IOT_MANAGER_LINK_WIFI, payload, payloadLen);
        LOG_I(WIRELESS_LOG_TAG, "mqtt rx len=%u", (unsigned int)payloadLen);
        return;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTSUB:")) {
        gWirelessIotMqttSubPending = false;
        gWirelessIotMqttSubReady = true;
    }
    if (wirelessMatchPrefix(lineBuf, lineLen, "+QMTSUB: 0,1,0")) {
        gWirelessIotMqttSubPending = false;
        gWirelessIotMqttSubReady = true;
    }
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

static void wirelessResetIotRuntime(void)
{
    gWirelessIotHttpPending = false;
    gWirelessIotMqttUserCfgPending = false;
    gWirelessIotMqttConnPending = false;
    gWirelessIotMqttQueryPending = false;
    gWirelessIotMqttSubPending = false;
    gWirelessIotMqttUserCfgReady = false;
    gWirelessIotMqttReady = false;
    gWirelessIotMqttSubReady = false;
    gWirelessIotMqttConnectedUrcSeen = false;
    gWirelessIotMqttState = 0U;
    gWirelessIotMqttCfgStep = 0U;
    gWirelessIotState = WIRELESS_IOT_IDLE;
}

static void wirelessStoreWifiRx(const uint8_t *buffer, uint16_t length)
{
    uint16_t index;

    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    if (length >= (uint16_t)sizeof(gWirelessWifiRxBuffer)) {
        buffer = &buffer[length - (uint16_t)sizeof(gWirelessWifiRxBuffer)];
        length = (uint16_t)sizeof(gWirelessWifiRxBuffer);
    }

    repRtosEnterCritical();
    for (index = 0U; index < length; index++) {
        if (gWirelessWifiRxUsed >= (uint16_t)sizeof(gWirelessWifiRxBuffer)) {
            gWirelessWifiRxTail = (uint16_t)((gWirelessWifiRxTail + 1U) % (uint16_t)sizeof(gWirelessWifiRxBuffer));
            gWirelessWifiRxUsed--;
        }
        gWirelessWifiRxBuffer[gWirelessWifiRxHead] = buffer[index];
        gWirelessWifiRxHead = (uint16_t)((gWirelessWifiRxHead + 1U) % (uint16_t)sizeof(gWirelessWifiRxBuffer));
        gWirelessWifiRxUsed++;
    }
    repRtosExitCritical();
}

static bool wirelessSubmitSimpleCommand(const char *cmdText)
{
    return fc41dSubmitTextCommand(WIRELESS_FC41D_DEVICE, cmdText) == FC41D_STATUS_OK;
}

static void wirelessServiceWifi(void)
{
    const stFc41dInfo *info;
    eFc41dStatus status;

    if ((gWirelessMode != WIRELESS_MODE_WIFI) || !fc41dIsReady(WIRELESS_FC41D_DEVICE)) {
        return;
    }

    info = fc41dGetInfo(WIRELESS_FC41D_DEVICE);
    if ((info == NULL) || info->isBusy || !gWirelessWifiEnabled || !gWirelessWifiConfigValid || gWirelessWifiGotIp ||
        (gWirelessWifiState == WIRELESS_WIFI_WAITING_CONNECTION)) {
        return;
    }

    if (gWirelessWifiJoinPending) {
        if (!info->isBusy && info->hasLastResult) {
            gWirelessWifiJoinPending = false;
            if (info->lastResult == FLOWPARSER_RESULT_OK) {
                gWirelessWifiState = WIRELESS_WIFI_WAITING_CONNECTION;
            } else {
                gWirelessWifiState = WIRELESS_WIFI_READY;
            }
        }
        return;
    }

    status = fc41dWifiBuildJoinCommand(gWirelessWifiSsid, gWirelessWifiPassword, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText));
    if (status == FC41D_STATUS_OK) {
        status = fc41dSubmitTextCommandEx(WIRELESS_FC41D_DEVICE, gWirelessCommandText, wirelessWifiJoinLineHandler, NULL);
    }
    if (status == FC41D_STATUS_OK) {
        gWirelessWifiJoinPending = true;
        gWirelessWifiState = WIRELESS_WIFI_WAITING_CONNECTION;
    } else if ((status != FC41D_STATUS_BUSY) && (status != FC41D_STATUS_NOT_READY)) {
        gWirelessWifiState = WIRELESS_WIFI_ERROR;
    }
}

static bool wirelessSubmitHttpAuth(void)
{
    if (fc41dHttpBuildHeaderCommand(gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }
    if (fc41dSubmitTextCommandEx(WIRELESS_FC41D_DEVICE, gWirelessCommandText, wirelessIotHttpLineHandler, NULL) == FC41D_STATUS_OK) {
        gWirelessIotHttpStep = 1U;
        gWirelessIotHttpPending = true;
        gWirelessIotState = WIRELESS_IOT_WAIT_AUTH;
        return true;
    }
    return false;
}

static bool wirelessSubmitHttpUrl(void)
{
    if (fc41dHttpBuildUrlCommand(gWirelessIotHttpUrl, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }
    if (fc41dSubmitTextCommandEx(WIRELESS_FC41D_DEVICE, gWirelessCommandText, wirelessIotHttpLineHandler, NULL) == FC41D_STATUS_OK) {
        gWirelessIotHttpStep = 2U;
        gWirelessIotHttpPending = true;
        return true;
    }
    return false;
}

static bool wirelessSubmitHttpPost(void)
{
    uint16_t payloadLen;

    if (!wirelessBuildHttpAuthPayload(gWirelessIotHttpPayload, (uint16_t)sizeof(gWirelessIotHttpPayload))) {
        return false;
    }
    payloadLen = (uint16_t)strlen(gWirelessIotHttpPayload);
    if (fc41dHttpBuildPostJsonCommand(gWirelessIotHttpUrl, payloadLen, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }

    gWirelessIotHttpResponseLen = 0U;
    gWirelessIotHttpResponse[0] = '\0';
    if (fc41dSubmitPromptCommandEx(WIRELESS_FC41D_DEVICE,
                                   gWirelessCommandText,
                                   (const uint8_t *)gWirelessIotHttpPayload,
                                   payloadLen,
                                   wirelessIotHttpLineHandler,
                                   NULL) == FC41D_STATUS_OK) {
        gWirelessIotHttpStep = 3U;
        gWirelessIotHttpPending = true;
        return true;
    }
    return false;
}

static bool wirelessSubmitHttpRead(void)
{
    if (fc41dHttpBuildReadCommand(gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }
    gWirelessIotHttpResponseLen = 0U;
    gWirelessIotHttpResponse[0] = '\0';
    if (fc41dSubmitTextCommandEx(WIRELESS_FC41D_DEVICE, gWirelessCommandText, wirelessIotHttpLineHandler, NULL) == FC41D_STATUS_OK) {
        gWirelessIotHttpStep = 4U;
        gWirelessIotHttpPending = true;
        return true;
    }
    return false;
}

static bool wirelessSubmitMqttUserCfg(void)
{
    eFc41dStatus status;

    if (!wirelessBuildMqttLogin(gWirelessMqttUsername, (uint16_t)sizeof(gWirelessMqttUsername), gWirelessMqttPassword, (uint16_t)sizeof(gWirelessMqttPassword))) {
        return false;
    }

    if (gWirelessIotMqttCfgStep == 0U) {
        status = fc41dMqttBuildUserCfgCommand(gWirelessIotSn,
                                              gWirelessMqttUsername,
                                              gWirelessMqttPassword,
                                              gWirelessCommandText,
                                              (uint16_t)sizeof(gWirelessCommandText));
    } else if (gWirelessIotMqttCfgStep == 1U) {
        status = fc41dMqttBuildRecvModeCommand(0U, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText));
    } else {
        status = fc41dMqttBuildConnectCommand(gWirelessIotMqttHost,
                                              gWirelessIotMqttPort,
                                              gWirelessCommandText,
                                              (uint16_t)sizeof(gWirelessCommandText));
    }

    if (status != FC41D_STATUS_OK) {
        return false;
    }

    if (fc41dSubmitTextCommand(WIRELESS_FC41D_DEVICE, gWirelessCommandText) == FC41D_STATUS_OK) {
        gWirelessIotMqttUserCfgPending = true;
        gWirelessIotState = WIRELESS_IOT_MQTT_CONNECTING;
        return true;
    }
    return false;
}

static bool wirelessSubmitMqttConnect(void)
{
    if (fc41dMqttBuildLoginCommand(gWirelessIotSn, gWirelessMqttUsername, gWirelessMqttPassword, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }
    if (fc41dSubmitTextCommand(WIRELESS_FC41D_DEVICE, gWirelessCommandText) == FC41D_STATUS_OK) {
        gWirelessIotMqttConnPending = true;
        gWirelessIotState = WIRELESS_IOT_MQTT_CONNECTING;
        return true;
    }
    return false;
}

static bool wirelessSubmitMqttSubscribe(void)
{
    if (fc41dMqttBuildSubscribeCommand(gWirelessIotMqttSubTopic, 1U, gWirelessCommandText, (uint16_t)sizeof(gWirelessCommandText)) != FC41D_STATUS_OK) {
        return false;
    }
    if (fc41dSubmitTextCommand(WIRELESS_FC41D_DEVICE, gWirelessCommandText) == FC41D_STATUS_OK) {
        gWirelessIotMqttSubPending = true;
        return true;
    }
    return false;
}

static void wirelessServiceIot(void)
{
    const stFc41dInfo *info;
    uint32_t nowTick;

    if (!gWirelessMqttEnabled) {
        return;
    }
    if (gWirelessWifiState != WIRELESS_WIFI_CONNECTED) {
        gWirelessIotState = WIRELESS_IOT_WAIT_WIFI;
        return;
    }

    info = fc41dGetInfo(WIRELESS_FC41D_DEVICE);
    if ((info == NULL) || info->isBusy) {
        return;
    }
    nowTick = repRtosGetTickMs();

    if (gWirelessIotHttpPending) {
        gWirelessIotHttpPending = false;
        if (!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) {
            gWirelessIotState = WIRELESS_IOT_ERROR;
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        } else if (gWirelessIotHttpStep == 1U) {
            (void)wirelessSubmitHttpUrl();
        } else if (gWirelessIotHttpStep == 2U) {
            (void)wirelessSubmitHttpPost();
        } else if (gWirelessIotHttpStep == 3U) {
            (void)wirelessSubmitHttpRead();
        } else if (!wirelessTryParseHttpKey()) {
            gWirelessIotHttpStep = 0U;
            gWirelessIotState = WIRELESS_IOT_ERROR;
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        } else {
            gWirelessIotHttpStep = 0U;
        }
        return;
    }
    if (gWirelessIotMqttUserCfgPending) {
        gWirelessIotMqttUserCfgPending = false;
        if (info->hasLastResult && (info->lastResult == FLOWPARSER_RESULT_OK)) {
            gWirelessIotMqttCfgStep++;
            gWirelessIotMqttUserCfgReady = gWirelessIotMqttCfgStep >= 3U;
            if (gWirelessIotMqttUserCfgReady) {
                gWirelessIotMqttCfgStep = 0U;
            }
        } else {
            gWirelessIotMqttCfgStep = 0U;
            gWirelessIotMqttUserCfgReady = false;
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        }
        return;
    }
    if (gWirelessIotMqttConnPending) {
        gWirelessIotMqttConnPending = false;
        if ((info->hasLastResult && (info->lastResult == FLOWPARSER_RESULT_OK)) || gWirelessIotMqttConnectedUrcSeen) {
            gWirelessIotMqttUserCfgReady = true;
            gWirelessIotMqttConnectedUrcSeen = true;
            gWirelessIotMqttReady = true;
            gWirelessIotMqttState = 4U;
            gWirelessIotState = WIRELESS_IOT_MQTT_READY;
            LOG_I(WIRELESS_LOG_TAG, "mqtt broker connected");
        } else {
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        }
        return;
    }
    if (gWirelessIotMqttQueryPending) {
        gWirelessIotMqttQueryPending = false;
        return;
    }
    if (gWirelessIotMqttSubPending) {
        gWirelessIotMqttSubPending = false;
        if (info->hasLastResult && (info->lastResult == FLOWPARSER_RESULT_OK)) {
            gWirelessIotMqttSubReady = true;
        }
        return;
    }
    if ((int32_t)(nowTick - gWirelessIotNextRetryTick) < 0) {
        return;
    }
    if (!gWirelessIotKeyReady) {
        if (!wirelessSubmitHttpAuth()) {
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        }
        return;
    }
    if (!gWirelessIotMqttUserCfgReady) {
        if (!wirelessSubmitMqttUserCfg()) {
            gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        }
        return;
    }
    if (!gWirelessIotMqttReady && !wirelessSubmitMqttConnect()) {
        gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
        return;
    }
    if (gWirelessIotMqttReady && !gWirelessIotMqttSubReady && !wirelessSubmitMqttSubscribe()) {
        gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
    }
}

static void wirelessServiceProtocol(void)
{
    uint16_t length;

    if (gWirelessMode == WIRELESS_MODE_BLE) {
        length = wirelessReadBleData(gWirelessProtocolRxBuffer, (uint16_t)sizeof(gWirelessProtocolRxBuffer));
        if (length > 0U) {
            (void)protcolMgrPushReceivedData(IOT_MANAGER_LINK_BLE, gWirelessProtocolRxBuffer, length);
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
    runtime.peerConnected = (state != NULL) && state->isBleConnected;
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

static void wirelessServicePrioritySwitch(void)
{
    const stFc41dState *state;
    const stFc41dInfo *info;
    uint32_t nowTick;
    eFc41dStatus status;

    if (!gWirelessWifiPrioritySwitchPending) {
        return;
    }

    nowTick = repRtosGetTickMs();
    if ((int32_t)(nowTick - gWirelessWifiPrioritySwitchTick) < 0) {
        return;
    }

    if (gWirelessMode == WIRELESS_MODE_BLE) {
        state = fc41dGetState(WIRELESS_FC41D_DEVICE);
        info = fc41dGetInfo(WIRELESS_FC41D_DEVICE);
        if ((state != NULL) && state->isBleConnected) {
            if (!gWirelessWifiPriorityDisconnectPending && ((info == NULL) || !info->isBusy)) {
                status = fc41dDisconnectBle(WIRELESS_FC41D_DEVICE);
                if (status == FC41D_STATUS_OK) {
                    gWirelessWifiPriorityDisconnectPending = true;
                    gWirelessWifiPriorityDisconnectDeadline = nowTick + WIRELESS_BLE_DISCONNECT_WAIT_MS;
                }
            }
            if (!gWirelessWifiPriorityDisconnectPending ||
                ((int32_t)(nowTick - gWirelessWifiPriorityDisconnectDeadline) < 0)) {
                return;
            }
        }
    }

    gWirelessBleEnabled = false;
    gWirelessWifiEnabled = gWirelessWifiConfigValid;
    gWirelessMqttEnabled = gWirelessWifiConfigValid;
    gWirelessTargetMode = gWirelessWifiConfigValid ? WIRELESS_MODE_WIFI : WIRELESS_MODE_BLE;
    gWirelessWifiPrioritySwitchPending = false;
    gWirelessWifiPriorityDisconnectPending = false;
    LOG_I(WIRELESS_LOG_TAG, "priority switch to wifi cfg=%u", gWirelessWifiConfigValid ? 1U : 0U);
}

static eWirelessMode wirelessResolveTargetMode(void)
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
    if (!wirelessConfigureIfNeeded()) {
        return false;
    }
    (void)wirelessLoadStorageConfig();
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

    if (!wirelessConfigureIfNeeded()) {
        return;
    }
    (void)wirelessLoadStorageConfig();
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

eWirelessWifiState wirelessGetWifiState(void)
{
    return gWirelessWifiState;
}

eWirelessIotState wirelessGetIotState(void)
{
    return gWirelessIotState;
}

bool wirelessSetBleEnabled(bool enabled)
{
    gWirelessBleEnabled = enabled;
    if (enabled) {
        gWirelessWifiEnabled = false;
        gWirelessMqttEnabled = false;
    } else if (!gWirelessWifiEnabled && gWirelessWifiConfigValid) {
        gWirelessWifiEnabled = true;
        gWirelessMqttEnabled = true;
    }
    gWirelessTargetMode = wirelessResolveTargetMode();
    return true;
}

bool wirelessGetBleEnabled(void)
{
    return gWirelessBleEnabled;
}

bool wirelessSetWifiEnabled(bool enabled)
{
    if (enabled && !gWirelessWifiConfigValid) {
        return false;
    }

    gWirelessWifiEnabled = enabled;
    if (enabled) {
        gWirelessBleEnabled = false;
        gWirelessMqttEnabled = true;
        gWirelessTargetMode = WIRELESS_MODE_WIFI;
        gWirelessWifiState = WIRELESS_WIFI_INITIALIZING;
    } else {
        gWirelessBleEnabled = true;
        gWirelessMqttEnabled = false;
        gWirelessTargetMode = WIRELESS_MODE_BLE;
        gWirelessWifiState = WIRELESS_WIFI_IDLE;
        wirelessResetIotRuntime();
    }
    return true;
}

bool wirelessGetWifiEnabled(void)
{
    return gWirelessWifiEnabled;
}

bool wirelessSetMqttEnabled(bool enabled)
{
    if (enabled && (gWirelessWifiState != WIRELESS_WIFI_CONNECTED)) {
        return false;
    }
    gWirelessMqttEnabled = enabled;
    if (!enabled) {
        (void)wirelessSubmitSimpleCommand("AT+MQTTCLEAN=0\r\n");
        wirelessResetIotRuntime();
    }
    return true;
}

bool wirelessGetMqttEnabled(void)
{
    return gWirelessMqttEnabled;
}

bool wirelessRequestWifiPrioritySwitch(void)
{
    if (!gWirelessWifiConfigValid) {
        return false;
    }

    gWirelessWifiPrioritySwitchPending = true;
    gWirelessWifiPriorityDisconnectPending = false;
    gWirelessWifiPrioritySwitchTick = repRtosGetTickMs() + WIRELESS_MODE_SWITCH_DELAY_MS;
    return true;
}

bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen)
{
    if ((ssid == NULL) || (ssidLen > WIRELESS_WIFI_SSID_MAX_LEN) ||
        ((password == NULL) && (passwordLen > 0U)) || (passwordLen > WIRELESS_WIFI_PASSWORD_MAX_LEN)) {
        return false;
    }
    if (!wirelessCopyBytesAsText(gWirelessWifiSsid, (uint16_t)sizeof(gWirelessWifiSsid), ssid, ssidLen) ||
        !wirelessCopyBytesAsText(gWirelessWifiPassword,
                                 (uint16_t)sizeof(gWirelessWifiPassword),
                                 (passwordLen > 0U) ? password : (const uint8_t *)"",
                                 passwordLen)) {
        return false;
    }
    gWirelessWifiConfigValid = ssidLen > 0U;
    if ((gWirelessMode == WIRELESS_MODE_BLE) || gWirelessBleEnabled) {
        gWirelessBleEnabled = true;
        gWirelessWifiEnabled = false;
        gWirelessMqttEnabled = false;
        gWirelessTargetMode = WIRELESS_MODE_BLE;
    } else {
        gWirelessBleEnabled = !gWirelessWifiConfigValid;
        gWirelessWifiEnabled = gWirelessWifiConfigValid;
        gWirelessMqttEnabled = gWirelessWifiConfigValid;
        gWirelessTargetMode = wirelessResolveTargetMode();
    }
    gWirelessWifiGotIp = false;
    gWirelessWifiJoinPending = false;
    gWirelessWifiState = ((gWirelessTargetMode == WIRELESS_MODE_WIFI) && gWirelessWifiConfigValid) ?
        WIRELESS_WIFI_INITIALIZING : WIRELESS_WIFI_IDLE;
    if (gWirelessWifiConfigValid) {
        (void)memoryMkdir(WIRELESS_NET_DIR_PATH);
        (void)wirelessWriteTextFile(WIRELESS_WIFI_SSID_PATH, gWirelessWifiSsid);
        (void)wirelessWriteTextFile(WIRELESS_WIFI_PASSWORD_PATH, gWirelessWifiPassword);
    }
    return true;
}

bool wirelessSendBleData(const uint8_t *buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U) || (gWirelessMode != WIRELESS_MODE_BLE)) {
        return false;
    }

    return fc41dWriteData(WIRELESS_FC41D_DEVICE, buffer, length) == FC41D_STATUS_OK;
}

uint16_t wirelessGetBleRxLength(void)
{
    if (gWirelessMode != WIRELESS_MODE_BLE) {
        return 0U;
    }

    return fc41dGetRxLength(WIRELESS_FC41D_DEVICE);
}

uint16_t wirelessReadBleData(uint8_t *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize == 0U) || (gWirelessMode != WIRELESS_MODE_BLE)) {
        return 0U;
    }

    return fc41dReadData(WIRELESS_FC41D_DEVICE, buffer, bufferSize);
}

bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length)
{
    char cmdText[WIRELESS_IOT_CMD_MAX_LEN];
    const stFc41dInfo *info;

    if ((buffer == NULL) || (length == 0U) || (length > WIRELESS_TX_BUFFER_SIZE) ||
        !gWirelessIotMqttReady || (gWirelessIotMqttTopic[0] == '\0')) {
        return false;
    }
    info = fc41dGetInfo(WIRELESS_FC41D_DEVICE);
    if ((info == NULL) || info->isBusy) {
        return false;
    }
    if (fc41dMqttBuildPublishRawCommand(gWirelessIotMqttTopic, length, 0U, 0U, cmdText, (uint16_t)sizeof(cmdText)) != FC41D_STATUS_OK) {
        return false;
    }
    (void)memcpy(gWirelessMqttTxBuffer, buffer, length);
    gWirelessMqttTxLen = length;
    return fc41dSubmitPromptCommandEx(WIRELESS_FC41D_DEVICE, cmdText, gWirelessMqttTxBuffer, gWirelessMqttTxLen, NULL, NULL) == FC41D_STATUS_OK;
}

uint16_t wirelessGetWifiRxLength(void)
{
    uint16_t length;

    repRtosEnterCritical();
    length = gWirelessWifiRxUsed;
    repRtosExitCritical();
    return length;
}

uint16_t wirelessReadWifiData(uint8_t *buffer, uint16_t bufferSize)
{
    uint16_t readLen;
    uint16_t index;

    if ((buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    repRtosEnterCritical();
    readLen = (bufferSize < gWirelessWifiRxUsed) ? bufferSize : gWirelessWifiRxUsed;
    for (index = 0U; index < readLen; index++) {
        buffer[index] = gWirelessWifiRxBuffer[gWirelessWifiRxTail];
        gWirelessWifiRxTail = (uint16_t)((gWirelessWifiRxTail + 1U) % (uint16_t)sizeof(gWirelessWifiRxBuffer));
    }
    gWirelessWifiRxUsed = (uint16_t)(gWirelessWifiRxUsed - readLen);
    repRtosExitCritical();
    return readLen;
}

bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize)
{
    return fc41dGetCachedMac(WIRELESS_FC41D_DEVICE, buffer, bufferSize);
}

/**************************End of file********************************/
