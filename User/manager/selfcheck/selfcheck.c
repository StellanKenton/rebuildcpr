/***********************************************************************************
* @file     : selfcheck.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "selfcheck.h"

static stSelfCheckStatus gSelfCheckStatus = {
    .lifecycle = {
        .classType = eSERVICE_LIFECYCLE_CLASS_RECOVERABLE_SERVICE,
    },
};

bool selfCheckInit(void)
{
    return lifecycleInit(&gSelfCheckStatus.lifecycle);
}

bool selfCheckStart(void)
{
    if (!selfCheckInit()) {
        return false;
    }

    if (!lifecycleStart(&gSelfCheckStatus.lifecycle)) {
        return false;
    }

    selfCheckReset();
    return true;
}

void selfCheckReset(void)
{
    gSelfCheckStatus.summary.expanderReady = false;
    gSelfCheckStatus.summary.displayReady = false;
    gSelfCheckStatus.summary.flashReady = false;
    gSelfCheckStatus.summary.motionReady = false;
    gSelfCheckStatus.summary.powerReady = false;
    gSelfCheckStatus.summary.updateReady = false;
    gSelfCheckStatus.summary.hasRun = false;
    gSelfCheckStatus.summary.isPassed = false;
}

void selfCheckSetExpanderResult(bool isPassed)
{
    gSelfCheckStatus.summary.expanderReady = isPassed;
}

void selfCheckSetDisplayResult(bool isPassed)
{
    gSelfCheckStatus.summary.displayReady = isPassed;
}

void selfCheckSetFlashResult(bool isPassed)
{
    gSelfCheckStatus.summary.flashReady = isPassed;
}

void selfCheckSetMotionResult(bool isPassed)
{
    gSelfCheckStatus.summary.motionReady = isPassed;
}

void selfCheckSetPowerResult(bool isPassed)
{
    gSelfCheckStatus.summary.powerReady = isPassed;
}

void selfCheckSetUpdateResult(bool isPassed)
{
    gSelfCheckStatus.summary.updateReady = isPassed;
}

bool selfCheckCommit(void)
{
    if (!lifecycleNoteProcess(&gSelfCheckStatus.lifecycle)) {
        return false;
    }

    gSelfCheckStatus.summary.hasRun = true;
    gSelfCheckStatus.summary.isPassed = gSelfCheckStatus.summary.expanderReady &&
                                        gSelfCheckStatus.summary.displayReady &&
                                        gSelfCheckStatus.summary.flashReady &&
                                        gSelfCheckStatus.summary.motionReady &&
                                        gSelfCheckStatus.summary.powerReady &&
                                        gSelfCheckStatus.summary.updateReady;

    if (gSelfCheckStatus.summary.isPassed) {
        (void)lifecycleStop(&gSelfCheckStatus.lifecycle);
    } else {
        lifecycleReportFault(&gSelfCheckStatus.lifecycle, eSERVICE_LIFECYCLE_ERROR_CHECK_FAILED);
    }

    return gSelfCheckStatus.summary.isPassed;
}

const stSelfCheckSummary *selfCheckGetSummary(void)
{
    return &gSelfCheckStatus.summary;
}

const stSelfCheckStatus *selfCheckGetStatus(void)
{
    return &gSelfCheckStatus;
}

/**************************End of file********************************/