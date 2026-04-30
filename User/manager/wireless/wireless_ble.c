/************************************************************************************
* @file     : wireless_ble.c
* @brief    : BLE defaults for the project wireless manager.
***********************************************************************************/
#include "wireless_ble.h"
#include "wireless.h"
#include "wireless_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../rep/module/fc41d/fc41d.h"

#define WIRELESS_BLE_DEVICE_TYPE           "CPRSensor"

static const char gWirelessBleServiceUuid[] = "FE60";
static const char gWirelessBleRxCharUuid[] = "FE61";
static const char gWirelessBleTxCharUuid[] = "FE62";

bool wirelessBleDefaultEnabled(void)
{
    return true;
}

void wirelessBleLoadDefaultConfig(stFc41dBleCfg *cfg)
{
    const char *sn;
    int nameLen;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->initMode = 2U;

    sn = wirelessGetIotSn();
    if (sn != NULL) {
        size_t snLen = strlen(sn);
        const char *shortSn = (snLen > 4U) ? (sn + snLen - 4U) : sn;
        nameLen = snprintf(cfg->name, sizeof(cfg->name),
                   "PRIMEDIC-%s-%s", WIRELESS_BLE_DEVICE_TYPE, shortSn);
    } else {
        nameLen = snprintf(cfg->name, sizeof(cfg->name),
                   "PRIMEDIC-%s", WIRELESS_BLE_DEVICE_TYPE);
    }

    if ((nameLen <= 0) || ((size_t)nameLen >= sizeof(cfg->name))) {
        (void)strncpy(cfg->name, "PRIMEDIC-" WIRELESS_BLE_DEVICE_TYPE, sizeof(cfg->name) - 1U);
    }

    (void)strncpy(cfg->serviceUuid, gWirelessBleServiceUuid, sizeof(cfg->serviceUuid) - 1U);
    (void)strncpy(cfg->rxCharUuid, gWirelessBleRxCharUuid, sizeof(cfg->rxCharUuid) - 1U);
    (void)strncpy(cfg->txCharUuid, gWirelessBleTxCharUuid, sizeof(cfg->txCharUuid) - 1U);
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

bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize == 0U)) {
        return false;
    }

    if (fc41dGetCachedMac(WIRELESS_FC41D_DEVICE, buffer, bufferSize)) {
        return true;
    }

    return wirelessCopyText(buffer, bufferSize, WIRELESS_FALLBACK_MAC_ADDRESS);
}

/**************************End of file********************************/
