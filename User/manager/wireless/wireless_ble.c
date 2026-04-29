/************************************************************************************
* @file     : wireless_ble.c
* @brief    : BLE defaults for the project wireless manager.
***********************************************************************************/
#include "wireless_ble.h"
#include "wireless.h"

#include <stdio.h>
#include <string.h>

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
/**************************End of file********************************/
