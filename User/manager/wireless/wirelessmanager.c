/***********************************************************************************
* @file     : wirelessmanager.c
* @brief    : Wireless manager service implementation.
* @details  : Owns the project bluetooth frameprocess instance lifecycle.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wirelessmanager.h"

#include <string.h>

#include <stddef.h>

#include "../../../rep/service/console/log.h"
#include "../../bsp/bsp_rtt.h"

#define WIRELESS_MANAGER_LOG_TAG "wirelessMgr"

static bool gWirelessManagerReady = false;
static void wirelessManagerRawTrace(const char *text)
{
    if (text == NULL) {
        return;
    }

    bspRttLogInit();
    LOG_I(WIRELESS_MANAGER_LOG_TAG, "%s", text);
}
static bool gWirelessManagerInitFailLogged = false;

static bool wirelessManagerEnsureReady(void)
{
    if (gWirelessManagerReady) {
        return true;
    }

    wirelessManagerRawTrace("wireless: init try\r\n");
    if (frmProcInit(FRAME_PROC0) != FRM_PROC_STATUS_OK) {
        if (!gWirelessManagerInitFailLogged) {
            LOG_E(WIRELESS_MANAGER_LOG_TAG, "frameprocess init fail");
            wirelessManagerRawTrace("wireless: init fail\r\n");
            gWirelessManagerInitFailLogged = true;
        }
        return false;
    }

    gWirelessManagerReady = true;
    gWirelessManagerInitFailLogged = false;
    wirelessManagerRawTrace("wireless: init ok\r\n");
    LOG_I(WIRELESS_MANAGER_LOG_TAG, "frameprocess init ok");
    return true;
}

bool wirelessManagerIsReady(void)
{
    return gWirelessManagerReady;
}

void wirelessManagerProcess(void)
{
    if (!wirelessManagerEnsureReady()) {
        return;
    }

    frmProcProcess(FRAME_PROC0);
}

eFrmProcStatus wirelessManagerPostSelfCheck(const stFrmDataTxSelfCheck *data, bool isUrgent)
{
    if (!wirelessManagerEnsureReady()) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostSelfCheck(FRAME_PROC0, data, isUrgent);
}

eFrmProcStatus wirelessManagerPostDisconnect(bool isUrgent)
{
    if (!wirelessManagerEnsureReady()) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostDisconnect(FRAME_PROC0, isUrgent);
}

eFrmProcStatus wirelessManagerPostCprData(const stFrmDataTxCprData *data, bool isUrgent)
{
    if (!wirelessManagerEnsureReady()) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostCprData(FRAME_PROC0, data, isUrgent);
}

const stFrmDataRxStore *wirelessManagerGetRxStore(void)
{
    if (!wirelessManagerEnsureReady()) {
        return NULL;
    }

    return frmProcGetRxStore(FRAME_PROC0);
}

void wirelessManagerClearRxFlags(uint32_t flags)
{
    if (!wirelessManagerEnsureReady()) {
        return;
    }

    frmProcClearRxFlags(FRAME_PROC0, flags);
}

/**************************End of file********************************/
