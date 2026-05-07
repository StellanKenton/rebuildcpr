/************************************************************************************
* @file     : power.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_POWER_H
#define REBUILDCPR_POWER_H

#include <stdbool.h>
#include <stdint.h>

#include "drvadc_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_VOLTAGE_UNIT_MV                  10U
#define POWER_VOLTAGE_TO_10MV(valueMv)         ((uint16_t)((valueMv) / POWER_VOLTAGE_UNIT_MV))

#define POWER_CHARGE_THRESHOLD_10MV            POWER_VOLTAGE_TO_10MV(4400U)
#define POWER_BATTERY_LOW_LEVEL_MAX            2U
#define POWER_BATTERY_FULL_LEVEL               5U
#define POWER_BATTERY_BOOT_SYNC_COUNT          12U
#define POWER_LED_BLINK_HALF_PERIOD_MS         500U

typedef struct stPowerChannelMap {
    eDrvAdcPortMap channel;
    uint16_t *rawValue;
    uint16_t *voltageValue;
} stPowerChannelMap;

typedef enum ePowerState {
    ePOWER_STATE_UNINIT = 0,
    ePOWER_STATE_READY,
    ePOWER_STATE_NORMAL,
    ePOWER_STATE_STOPPED,
    ePOWER_STATE_FAULT,
} ePowerState;

typedef struct stPowerStatus {
    ePowerState state;
    bool isShutDownRequested;
} stPowerStatus;

typedef enum ePowerChargeState {
    ePOWER_CHARGE_STATE_IDLE = 0,
    ePOWER_CHARGE_STATE_CHARGING,
    ePOWER_CHARGE_STATE_FULL,
} ePowerChargeState;

typedef struct PowerRaw {
    uint16_t battery;
    uint16_t dc;
    uint16_t v5v0;
    uint16_t v3v3;
} PowerRaw;

typedef struct PowerVoltage { /* 10mV unit */
    uint16_t batteryMv;
    uint16_t dcMv;
    uint16_t v5v0Mv;
    uint16_t v3v3Mv;
} PowerVoltage;

typedef struct PowerManager {
    stPowerStatus status;
    PowerRaw raw;
    PowerVoltage voltage;
    ePowerChargeState chargeState;
    bool isChargingStatusHigh;
    bool isChargeDoneStatusHigh;
    uint8_t BatLevel;
    uint8_t batLevelBootSyncCount;
    uint8_t batLevelBootSyncPeak;
    bool batLevelBootSyncActive;
} PowerManager;

bool powerInit(void);
bool powerIsReady(void);
void batholdon(void);
void batrelease(void);
uint16_t powerGetVoltage(eDrvAdcPortMap channel);
void powerBatteryUpdate(void);
uint8_t powerBatteryGet(void);
void powerLedProcess(void);
void powerTransRawToVoltage(void);
void powerProcess(void);
bool powerRequestShutDown(void);
bool powerRequestPowerUp(void);
const stPowerStatus *powerGetStatus(void);
const PowerManager *powerGetManager(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
