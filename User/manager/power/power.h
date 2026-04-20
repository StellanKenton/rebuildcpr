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

typedef struct PowerRaw {
    uint16_t battery;
    uint16_t dc;
    uint16_t v5v0;
    uint16_t v3v3;
} PowerRaw;

typedef struct PowerVoltage { // 10MV unit
    uint16_t batteryMv;
    uint16_t dcMv;
    uint16_t v5v0Mv;
    uint16_t v3v3Mv;
} PowerVoltage;

typedef struct PowerManager {
    stPowerStatus status;
    PowerRaw raw;
    PowerVoltage voltage;
} PowerManager;

bool powerInit(void);
uint16_t powerGetVoltage(eDrvAdcPortMap channel);
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
