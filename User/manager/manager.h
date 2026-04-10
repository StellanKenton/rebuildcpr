/************************************************************************************
* @file     : manager.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MANAGER_H
#define REBUILDCPR_MANAGER_H

#include <stdbool.h>

#include "power/power.h"
#include "selfcheck/selfcheck.h"
#include "update/update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stManagerStatus {
    bool isInitialized;
    bool expanderReady;
    bool displayReady;
    bool flashReady;
    bool motionReady;
    bool selfCheckDone;
    bool selfCheckPassed;
    bool powerActive;
    bool updateActive;
    const stSelfCheckSummary *selfCheck;
    const stPowerStatus *power;
    const stUpdateStatus *update;
} stManagerStatus;

bool managerInit(void);
bool managerIsInitialized(void);
bool managerRunStartupSelfCheck(void);
bool managerPowerStart(void);
void managerPowerStop(void);
void managerPowerProcess(void);
bool managerIsPowerActive(void);
bool managerUpdateStart(void);
void managerUpdateStop(void);
void managerUpdateProcess(void);
bool managerIsUpdateActive(void);
const stSelfCheckSummary *managerGetSelfCheckSummary(void);
const stManagerStatus *managerGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
