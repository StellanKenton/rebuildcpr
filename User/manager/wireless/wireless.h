/************************************************************************************
* @file     : wireless.h
* @brief    : Project-side wireless manager.
* @details  : Wraps the FC41D module for BLE initialization, periodic processing
*             and status query in the current product.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_H
#define REBUILDCPR_WIRELESS_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/module/fc41d/fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_LOG_TAG                     "wireless"
#define WIRELESS_FC41D_DEVICE                FC41D_DEV0
#define WIRELESS_BOOT_READY_TIMEOUT_MS       3000U
#define WIRELESS_WIFI_SSID_MAX_LEN           32U
#define WIRELESS_WIFI_PASSWORD_MAX_LEN       64U
#define WIRELESS_IOT_SN_MAX_LEN              32U
#define WIRELESS_IOT_KEY_MAX_LEN             96U
#define WIRELESS_IOT_URL_MAX_LEN             160U
#define WIRELESS_IOT_HOST_MAX_LEN            96U
#define WIRELESS_IOT_TOPIC_MAX_LEN           96U
#define WIRELESS_IOT_CMD_MAX_LEN             384U
#define WIRELESS_IOT_HTTP_PAYLOAD_LEN        192U
#define WIRELESS_IOT_HTTP_RESPONSE_LEN       512U

#define WIRELESS_DEV_DIR_PATH                "/dev"
#define WIRELESS_NET_DIR_PATH                "/net"
#define WIRELESS_WIFI_SSID_PATH              "/net/wifiname"
#define WIRELESS_WIFI_PASSWORD_PATH          "/net/wifipassword"
#define WIRELESS_IOT_SN_PATH                 "/dev/serial"
#define WIRELESS_IOT_HTTP_URL_PATH           "/net/iot_http_url"
#define WIRELESS_IOT_MQTT_KEY_PATH           "/net/mqttkey"
#define WIRELESS_IOT_MQTT_HOST_PATH          "/net/mqtt_host"
#define WIRELESS_IOT_MQTT_PORT_PATH          "/net/mqtt_port"
#define WIRELESS_IOT_MQTT_TOPIC_PATH         "/net/mqtt_topic"
#define WIRELESS_IOT_MQTT_SUB_TOPIC_PATH     "/net/mqtt_sub_topic"
#define WIRELESS_WIFI_DEFAULT_SSID           "rumi"
#define WIRELESS_WIFI_DEFAULT_PASSWORD       "1234567890"
#define WIRELESS_IOT_DEFAULT_SN              "HC630257T6001"
#define WIRELESS_IOT_DEFAULT_HTTP_URL        "http://iot-test2.yuwell.com:8800/device/secret-key"
#define WIRELESS_IOT_DEFAULT_MQTT_HOST       "iot-test2.yuwell.com"
#define WIRELESS_IOT_DEFAULT_MQTT_PORT       "1883"
#define WIRELESS_IOT_PRODUCT_SECRET          "YuWell@CPR"
#define WIRELESS_IOT_HTTP_RANDOM             "1234567812345678"
#define WIRELESS_IOT_RETRY_MS                5000U

typedef enum eWirelessState {
    eWIRELESS_STATE_INIT = 0,
    eWIRELESS_STATE_NORMAL,
    eWIRELESS_STATE_ERROR,
} eWirelessState;

typedef enum eWirelessMode {
    WIRELESS_MODE_BLE = 0,
    WIRELESS_MODE_WIFI,
} eWirelessMode;

typedef enum eWirelessWifiState {
    WIRELESS_WIFI_IDLE = 0,
    WIRELESS_WIFI_INITIALIZING,
    WIRELESS_WIFI_READY,
    WIRELESS_WIFI_WAITING_CONNECTION,
    WIRELESS_WIFI_CONNECTED,
    WIRELESS_WIFI_ERROR,
} eWirelessWifiState;

typedef enum eWirelessIotState {
    WIRELESS_IOT_IDLE = 0,
    WIRELESS_IOT_WAIT_WIFI,
    WIRELESS_IOT_WAIT_AUTH,
    WIRELESS_IOT_AUTH_READY,
    WIRELESS_IOT_MQTT_CONNECTING,
    WIRELESS_IOT_MQTT_READY,
    WIRELESS_IOT_ERROR,
} eWirelessIotState;

bool wirelessInit(void);
void wirelessProcess(void);
const eWirelessState *wirelessGetStatus(void);
eWirelessWifiState wirelessGetWifiState(void);
eWirelessIotState wirelessGetIotState(void);
bool wirelessSetBleEnabled(bool enabled);
bool wirelessSetWifiEnabled(bool enabled);
bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);
bool wirelessSetMqttEnabled(bool enabled);
bool wirelessGetBleEnabled(void);
bool wirelessGetWifiEnabled(void);
bool wirelessGetMqttEnabled(void);
bool wirelessRequestWifiPrioritySwitch(void);
bool wirelessRequestBleDisconnect(void);
bool wirelessSendBleData(const uint8_t *buffer, uint16_t length);
uint16_t wirelessGetBleRxLength(void);
uint16_t wirelessReadBleData(uint8_t *buffer, uint16_t bufferSize);
bool wirelessGetBleVersion(char *buffer, uint16_t bufferSize);
bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length);
uint16_t wirelessGetWifiRxLength(void);
uint16_t wirelessReadWifiData(uint8_t *buffer, uint16_t bufferSize);
bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize);
const char *wirelessGetIotSn(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
