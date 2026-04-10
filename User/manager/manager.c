/***********************************************************************************
* @file     : manager.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "manager.h"

#include "lsm6.h"
#include "pca9535_port.h"
#include "tm1651_port.h"
#include "w25qxxx.h"

static stManagerStatus gManagerStatus = {
    .isInitialized = false,
    .expanderReady = false,
    .displayReady = false,
    .flashReady = false,
    .motionReady = false,
    .selfCheckDone = false,
    .selfCheckPassed = false,
    .powerActive = false,
    .updateActive = false,
    .selfCheck = NULL,
    .power = NULL,
    .update = NULL,
};

static void managerRefreshStatus(void)
{
    gManagerStatus.selfCheck = selfCheckGetSummary();
    gManagerStatus.power = powerGetStatus();
    gManagerStatus.update = updateGetStatus();

    if (gManagerStatus.selfCheck != NULL) {
        gManagerStatus.selfCheckDone = gManagerStatus.selfCheck->hasRun;
        gManagerStatus.selfCheckPassed = gManagerStatus.selfCheck->isPassed;
    }

    if (gManagerStatus.power != NULL) {
        gManagerStatus.powerActive = gManagerStatus.power->lifecycle.isStarted &&
                                     !gManagerStatus.power->lifecycle.hasFault;
    }

    if (gManagerStatus.update != NULL) {
        gManagerStatus.updateActive = gManagerStatus.update->lifecycle.isStarted &&
                                      !gManagerStatus.update->lifecycle.hasFault;
    }
}

bool managerInit(void)
{
    if (gManagerStatus.isInitialized) {
        managerRefreshStatus();
        return true;
    }

    if (!selfCheckInit() || !powerInit() || !updateInit()) {
        return false;
    }

    gManagerStatus.isInitialized = true;
    managerRefreshStatus();
    return true;
}

bool managerIsInitialized(void)
{
    return gManagerStatus.isInitialized;
}

bool managerRunStartupSelfCheck(void)
{
    if (!managerInit() || !selfCheckStart()) {
        return false;
    }

    selfCheckReset();

    gManagerStatus.expanderReady = pca9535IsReady(PCA9535_DEV0);
    selfCheckSetExpanderResult(gManagerStatus.expanderReady);

    gManagerStatus.displayReady = tm1651IsReady(TM1651_DEV0);
    selfCheckSetDisplayResult(gManagerStatus.displayReady);

    gManagerStatus.flashReady = w25qxxxIsReady(W25QXXX_DEV0);
    selfCheckSetFlashResult(gManagerStatus.flashReady);

    gManagerStatus.motionReady = lsm6IsReady(LSM6_DEV0);
    selfCheckSetMotionResult(gManagerStatus.motionReady);

    selfCheckSetPowerResult(powerInit());
    selfCheckSetUpdateResult(updateInit());

    gManagerStatus.selfCheckPassed = selfCheckCommit();
    managerRefreshStatus();
    return gManagerStatus.selfCheckPassed;
}

bool managerPowerStart(void)
{
    if (!managerInit()) {
        return false;
    }

    if (managerIsPowerActive()) {
        return true;
    }

    if (!powerStart()) {
        managerRefreshStatus();
        return false;
    }

    managerRefreshStatus();
    return true;
}

void managerPowerStop(void)
{
    if (gManagerStatus.isInitialized) {
        powerStop();
        managerRefreshStatus();
    }
}

void managerPowerProcess(void)
{
    if (managerIsPowerActive()) {
        powerProcess();
        managerRefreshStatus();
    }
}

bool managerIsPowerActive(void)
{
    managerRefreshStatus();
    return gManagerStatus.powerActive;
}

bool managerUpdateStart(void)
{
    if (!managerInit()) {
        return false;
    }

    if (managerIsUpdateActive()) {
        return true;
    }

    if (!updateStart()) {
        managerRefreshStatus();
        return false;
    }

    managerRefreshStatus();
    return true;
}

void managerUpdateStop(void)
{
    if (gManagerStatus.isInitialized) {
        updateStop();
        managerRefreshStatus();
    }
}

void managerUpdateProcess(void)
{
    if (managerIsUpdateActive()) {
        updateProcess();
        managerRefreshStatus();
    }
}

bool managerIsUpdateActive(void)
{
    managerRefreshStatus();
    return gManagerStatus.updateActive;
}

const stSelfCheckSummary *managerGetSelfCheckSummary(void)
{
    managerRefreshStatus();
    return selfCheckGetSummary();
}

const stManagerStatus *managerGetStatus(void)
{
    managerRefreshStatus();
    return &gManagerStatus;
}

/**************************End of file********************************/
