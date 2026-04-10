/***********************************************************************************
* @file     : update.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update.h"

static stUpdateStatus gUpdateStatus = {
    .lifecycle = {
        .classType = eSERVICE_LIFECYCLE_CLASS_RECOVERABLE_SERVICE,
    },
    .state = eUPDATE_STATE_UNINIT,
    .isUpdateRequested = false,
};

bool updateInit(void)
{
    if (!lifecycleInit(&gUpdateStatus.lifecycle)) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return false;
    }

    if (gUpdateStatus.state == eUPDATE_STATE_UNINIT) {
        gUpdateStatus.state = eUPDATE_STATE_IDLE;
        gUpdateStatus.isUpdateRequested = false;
    }

    return true;
}

bool updateStart(void)
{
    if (!updateInit() || !lifecycleStart(&gUpdateStatus.lifecycle)) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return false;
    }

    gUpdateStatus.state = gUpdateStatus.isUpdateRequested ? eUPDATE_STATE_PENDING : eUPDATE_STATE_IDLE;
    return true;
}

void updateStop(void)
{
    if (!updateInit() || !lifecycleStop(&gUpdateStatus.lifecycle)) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return;
    }

    gUpdateStatus.state = eUPDATE_STATE_STOPPED;
    gUpdateStatus.isUpdateRequested = false;
}

void updateProcess(void)
{
    if (!lifecycleNoteProcess(&gUpdateStatus.lifecycle)) {
        if (gUpdateStatus.lifecycle.hasFault) {
            gUpdateStatus.state = eUPDATE_STATE_FAULT;
        }
        return;
    }

    gUpdateStatus.state = gUpdateStatus.isUpdateRequested ? eUPDATE_STATE_ACTIVE : eUPDATE_STATE_IDLE;
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

/**************************End of file********************************/