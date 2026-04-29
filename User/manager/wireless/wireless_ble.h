/************************************************************************************
* @file     : wireless_ble.h
* @brief    : BLE side of the project wireless manager.
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_BLE_H
#define REBUILDCPR_WIRELESS_BLE_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/module/fc41d/fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

bool wirelessBleDefaultEnabled(void);
void wirelessBleLoadDefaultConfig(stFc41dBleCfg *cfg);

bool wirelessGetBleEnabled(void);
bool wirelessSetBleEnabled(bool enabled);
bool wirelessSendBleData(const uint8_t *buffer, uint16_t length);
uint16_t wirelessGetBleRxLength(void);
uint16_t wirelessReadBleData(uint8_t *buffer, uint16_t bufferSize);
bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
