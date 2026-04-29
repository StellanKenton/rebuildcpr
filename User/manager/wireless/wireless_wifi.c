/************************************************************************************
* @file     : wireless_wifi.c
* @brief    : WiFi/MQTT defaults for the project wireless manager.
***********************************************************************************/
#include "wireless_wifi.h"

static const char gWirelessWifiDefaultSsid[] = "rumi";
static const char gWirelessWifiDefaultPassword[] = "1234567890";
static const char gWirelessIotDefaultSn[] = "HC630257T6001";
static const char gWirelessIotDefaultHttpUrl[] = "http://iot-test2.yuwell.com:8800/device/secret-key";
static const char gWirelessIotDefaultMqttHost[] = "iot-test2.yuwell.com";
static const char gWirelessIotDefaultMqttPort[] = "1883";

bool wirelessWifiDefaultEnabled(void)
{
    return false;
}

bool wirelessMqttDefaultEnabled(void)
{
    return false;
}

const char *wirelessWifiGetDefaultSsid(void)
{
    return gWirelessWifiDefaultSsid;
}

const char *wirelessWifiGetDefaultPassword(void)
{
    return gWirelessWifiDefaultPassword;
}

const char *wirelessWifiGetDefaultSn(void)
{
    return gWirelessIotDefaultSn;
}

const char *wirelessWifiGetDefaultHttpUrl(void)
{
    return gWirelessIotDefaultHttpUrl;
}

const char *wirelessWifiGetDefaultMqttHost(void)
{
    return gWirelessIotDefaultMqttHost;
}

const char *wirelessWifiGetDefaultMqttPortText(void)
{
    return gWirelessIotDefaultMqttPort;
}
/**************************End of file********************************/
