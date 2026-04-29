/************************************************************************************
* @file     : wireless_ble.c
* @brief    : BLE defaults for the project wireless manager.
***********************************************************************************/
#include "wireless_ble.h"

#include <string.h>

static const char gWirelessBleName[] = "rumi";
static const char gWirelessBleServiceUuid[] = "FE60";
static const char gWirelessBleRxCharUuid[] = "FE61";
static const char gWirelessBleTxCharUuid[] = "FE62";

bool wirelessBleDefaultEnabled(void)
{
    return true;
}

void wirelessBleLoadDefaultConfig(stFc41dBleCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->initMode = 2U;
    (void)strncpy(cfg->name, gWirelessBleName, sizeof(cfg->name) - 1U);
    (void)strncpy(cfg->serviceUuid, gWirelessBleServiceUuid, sizeof(cfg->serviceUuid) - 1U);
    (void)strncpy(cfg->rxCharUuid, gWirelessBleRxCharUuid, sizeof(cfg->rxCharUuid) - 1U);
    (void)strncpy(cfg->txCharUuid, gWirelessBleTxCharUuid, sizeof(cfg->txCharUuid) - 1U);
}
/**************************End of file********************************/
