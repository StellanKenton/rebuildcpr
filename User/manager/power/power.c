/***********************************************************************************
* @file     : power.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "power.h"

static stPowerStatus gPowerStatus = {
    .lifecycle = {
        .classType = eSERVICE_LIFECYCLE_CLASS_ACTIVE_SERVICE,
    },
    .state = ePOWER_STATE_UNINIT,
    .isLowPowerRequested = false,
};

bool powerInit(void)
{
    if (!lifecycleInit(&gPowerStatus.lifecycle)) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return false;
    }

    if (gPowerStatus.state == ePOWER_STATE_UNINIT) {
        gPowerStatus.state = ePOWER_STATE_READY;
        gPowerStatus.isLowPowerRequested = false;
    }

    return true;
}

bool powerStart(void)
{
    if (!powerInit() || !lifecycleStart(&gPowerStatus.lifecycle)) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return false;
    }

    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
    return true;
}

void powerStop(void)
{
    if (!powerInit() || !lifecycleStop(&gPowerStatus.lifecycle)) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return;
    }

    gPowerStatus.state = ePOWER_STATE_STOPPED;
}

void powerProcess(void)
{
    if (!lifecycleNoteProcess(&gPowerStatus.lifecycle)) {
        if (gPowerStatus.lifecycle.hasFault) {
            gPowerStatus.state = ePOWER_STATE_FAULT;
        }
        return;
    }

    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
}

bool powerRequestLowPower(bool isEnabled)
{
    if (!gPowerStatus.lifecycle.isReady || gPowerStatus.lifecycle.hasFault) {
        return false;
    }

    gPowerStatus.isLowPowerRequested = isEnabled;
    return true;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerStatus;
}

/**************************End of file********************************/
