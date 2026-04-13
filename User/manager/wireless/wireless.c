/***********************************************************************************
* @file     : wireless.c
* @brief    : Wireless service implementation.
* @details  : Owns project bluetooth and wifi frameprocess lifecycles.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <stddef.h>

#include "../../../rep/service/console/log.h"

#define WIRELESS_LOG_TAG "wireless"

typedef struct stWirelessRuntime {
    bool isReady;
    bool isInitFailLogged;
} stWirelessRuntime;

static stWirelessRuntime gWirelessRuntime[WIRELESS_LINK_MAX];

static const eFrmProcMapType gWirelessProcMap[WIRELESS_LINK_MAX] = {
    FRAME_PROC0,
    FRAME_PROC1,
};

static const char *gWirelessLinkName[WIRELESS_LINK_MAX] = {
    "ble",
    "wifi",
};

static bool wirelessIsValidLink(eWirelessLinkType link)
{
    return ((uint32_t)link < (uint32_t)WIRELESS_LINK_MAX);
}

static const char *wirelessGetLinkName(eWirelessLinkType link)
{
    if (!wirelessIsValidLink(link)) {
        return "unknown";
    }

    return gWirelessLinkName[link];
}

static eFrmProcMapType wirelessGetProc(eWirelessLinkType link)
{
    return gWirelessProcMap[link];
}

static bool wirelessEnsureReady(eWirelessLinkType link)
{
    stWirelessRuntime *lRuntime;
    eFrmProcStatus lStatus;

    if (!wirelessIsValidLink(link)) {
        return false;
    }

    lRuntime = &gWirelessRuntime[link];
    if (lRuntime->isReady) {
        return true;
    }

    lStatus = frmProcInit(wirelessGetProc(link));
    if (lStatus != FRM_PROC_STATUS_OK) {
        if (!lRuntime->isInitFailLogged) {
            LOG_W(WIRELESS_LOG_TAG,
                  "link=%s init fail status=%d",
                  wirelessGetLinkName(link),
                  (int)lStatus);
            lRuntime->isInitFailLogged = true;
        }
        return false;
    }

    lRuntime->isReady = true;
    lRuntime->isInitFailLogged = false;
    LOG_I(WIRELESS_LOG_TAG, "link=%s init ok", wirelessGetLinkName(link));
    return true;
}

bool wirelessIsReady(eWirelessLinkType link)
{
    return wirelessIsValidLink(link) && gWirelessRuntime[link].isReady;
}

void wirelessProcessLink(eWirelessLinkType link)
{
    if (!wirelessEnsureReady(link)) {
        return;
    }

    frmProcProcess(wirelessGetProc(link));
}

void wirelessProcess(void)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)WIRELESS_LINK_MAX; lIndex++) {
        wirelessProcessLink((eWirelessLinkType)lIndex);
    }
}

eFrmProcStatus wirelessPostSelfCheck(eWirelessLinkType link, const stFrmDataTxSelfCheck *data, bool isUrgent)
{
    if (!wirelessEnsureReady(link)) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostSelfCheck(wirelessGetProc(link), data, isUrgent);
}

eFrmProcStatus wirelessPostDisconnect(eWirelessLinkType link, bool isUrgent)
{
    if (!wirelessEnsureReady(link)) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostDisconnect(wirelessGetProc(link), isUrgent);
}

eFrmProcStatus wirelessPostCprData(eWirelessLinkType link, const stFrmDataTxCprData *data, bool isUrgent)
{
    if (!wirelessEnsureReady(link)) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    return frmProcPostCprData(wirelessGetProc(link), data, isUrgent);
}

const stFrmDataRxStore *wirelessGetRxStore(eWirelessLinkType link)
{
    if (!wirelessEnsureReady(link)) {
        return NULL;
    }

    return frmProcGetRxStore(wirelessGetProc(link));
}

void wirelessClearRxFlags(eWirelessLinkType link, uint32_t flags)
{
    if (!wirelessEnsureReady(link)) {
        return;
    }

    frmProcClearRxFlags(wirelessGetProc(link), flags);
}

/**************************End of file********************************/
