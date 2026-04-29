/************************************************************************************
* @file     : wireless_wifi.h
* @brief    : WiFi/MQTT side of the project wireless manager.
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_WIFI_H
#define REBUILDCPR_WIRELESS_WIFI_H

#include "wireless.h"

#ifdef __cplusplus
extern "C" {
#endif

bool wirelessWifiDefaultEnabled(void);
bool wirelessMqttDefaultEnabled(void);
const char *wirelessWifiGetDefaultSsid(void);
const char *wirelessWifiGetDefaultPassword(void);
const char *wirelessWifiGetDefaultSn(void);
const char *wirelessWifiGetDefaultHttpUrl(void);
const char *wirelessWifiGetDefaultMqttHost(void);
const char *wirelessWifiGetDefaultMqttPortText(void);

eWirelessWifiState wirelessGetWifiState(void);
eWirelessIotState wirelessGetIotState(void);
bool wirelessGetWifiEnabled(void);
bool wirelessSetWifiEnabled(bool enabled);
bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);
bool wirelessGetMqttEnabled(void);
bool wirelessSetMqttEnabled(bool enabled);
bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length);
uint16_t wirelessGetWifiRxLength(void);
uint16_t wirelessReadWifiData(uint8_t *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
