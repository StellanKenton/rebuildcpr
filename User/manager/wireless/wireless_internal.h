/************************************************************************************
* @file     : wireless_internal.h
* @brief    : Internal shared state for the project wireless manager.
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_INTERNAL_H
#define REBUILDCPR_WIRELESS_INTERNAL_H

#include "wireless.h"

#include "../iotmanager/cprsensor_protocol.h"
#include "../../../rep/module/fc41d/fc41d.h"
#include "../../../rep/tools/md5/md5.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_RETRY_LOG_MS            1000U
#define WIRELESS_TX_BUFFER_SIZE          640U
#define WIRELESS_IOT_MD5_HEX_LEN         33U
#define WIRELESS_MODE_SWITCH_DELAY_MS    500U
#define WIRELESS_BLE_DISCONNECT_WAIT_MS  2000U
#define WIRELESS_ROLE_SWITCH_SETTLE_MS   500U
#define WIRELESS_AES_KEY_SIZE            MD5_DIGEST_SIZE
#define WIRELESS_FALLBACK_MAC_ADDRESS    "00:11:22:33:44:55"

extern eWirelessState gWirelessState;
extern eWirelessWifiState gWirelessWifiState;
extern eWirelessIotState gWirelessIotState;
extern bool gWirelessConfigured;
extern bool gWirelessStarted;
extern bool gWirelessBleEnabled;
extern bool gWirelessWifiEnabled;
extern bool gWirelessMqttEnabled;
extern bool gWirelessWifiConfigValid;
extern bool gWirelessWifiConnected;
extern bool gWirelessWifiGotIp;
extern bool gWirelessWifiJoinPending;
extern bool gWirelessIotStorageLoaded;
extern bool gWirelessIotKeyReady;
extern bool gWirelessIotHttpPending;
extern bool gWirelessIotMqttUserCfgPending;
extern bool gWirelessIotMqttConnPending;
extern bool gWirelessIotMqttQueryPending;
extern bool gWirelessIotMqttSubPending;
extern bool gWirelessIotMqttUserCfgReady;
extern bool gWirelessIotMqttReady;
extern bool gWirelessIotMqttSubReady;
extern bool gWirelessIotMqttConnectedUrcSeen;
extern bool gWirelessRoleStartPending;
extern bool gWirelessWifiPrioritySwitchPending;
extern bool gWirelessWifiPriorityDisconnectPending;
extern bool gWirelessBleDisconnectPending;
extern bool gWirelessCipherReady;
extern bool gWirelessProtocolHandshakeDone;
extern uint8_t gWirelessIotMqttState;
extern uint8_t gWirelessIotMqttCfgStep;
extern uint8_t gWirelessIotHttpStep;
extern uint32_t gWirelessLastWarnTick;
extern uint32_t gWirelessIotNextRetryTick;
extern uint32_t gWirelessRoleStartTick;
extern uint32_t gWirelessWifiPrioritySwitchTick;
extern uint32_t gWirelessWifiPriorityDisconnectDeadline;
extern uint32_t gWirelessBleDisconnectDeadline;
extern eWirelessMode gWirelessMode;
extern eWirelessMode gWirelessTargetMode;
extern char gWirelessWifiSsid[WIRELESS_WIFI_SSID_MAX_LEN + 1U];
extern char gWirelessWifiPassword[WIRELESS_WIFI_PASSWORD_MAX_LEN + 1U];
extern char gWirelessIotSn[WIRELESS_IOT_SN_MAX_LEN + 1U];
extern char gWirelessIotKey[WIRELESS_IOT_KEY_MAX_LEN + 1U];
extern char gWirelessIotHttpUrl[WIRELESS_IOT_URL_MAX_LEN + 1U];
extern char gWirelessIotMqttHost[WIRELESS_IOT_HOST_MAX_LEN + 1U];
extern char gWirelessIotMqttTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
extern char gWirelessIotMqttSubTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
extern char gWirelessCommandText[WIRELESS_IOT_CMD_MAX_LEN];
extern char gWirelessMqttUsername[64];
extern char gWirelessMqttPassword[WIRELESS_IOT_MD5_HEX_LEN];
extern char gWirelessIotHttpPayload[WIRELESS_IOT_HTTP_PAYLOAD_LEN];
extern char gWirelessIotHttpResponse[WIRELESS_IOT_HTTP_RESPONSE_LEN + 1U];
extern uint16_t gWirelessIotHttpResponseLen;
extern uint16_t gWirelessIotMqttPort;
extern uint8_t gWirelessMqttTxBuffer[WIRELESS_TX_BUFFER_SIZE];
extern uint16_t gWirelessMqttTxLen;
extern uint8_t gWirelessWifiRxBuffer[WIRELESS_TX_BUFFER_SIZE];
extern uint8_t gWirelessProtocolRxBuffer[WIRELESS_TX_BUFFER_SIZE];
extern uint8_t gWirelessAesKey[WIRELESS_AES_KEY_SIZE];
extern uint16_t gWirelessWifiRxHead;
extern uint16_t gWirelessWifiRxTail;
extern uint16_t gWirelessWifiRxUsed;

bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text);
bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen);
void wirelessTrimText(char *text);
bool wirelessIsValidSnText(const char *text);
bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize);
bool wirelessWriteTextFile(const char *path, const char *text);
bool wirelessTryParseU16Text(const char *text, uint16_t *value);
bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text);
eWirelessMode wirelessResolveTargetMode(void);

bool wirelessLoadStorageConfig(void);
void wirelessResetIotRuntime(void);
void wirelessServiceWifi(void);
void wirelessServiceIot(void);
void wirelessServicePrioritySwitch(void);
bool wirelessFc41dUrcMatcher(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
void wirelessFc41dUrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
