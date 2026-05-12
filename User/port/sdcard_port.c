/***********************************************************************************
* @file     : sdcard_port.c
* @brief    : Project-side SD card binding implementation.
* @details  : Keeps SD card transport unbound in the current product while
*             exposing an explicit ops table so the reusable core no longer
*             relies on weak platform hooks.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sdcard_port.h"

static void sdcardPortLoadDefaultCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->initTimeoutMs = SDCARD_DEFAULT_INIT_TIMEOUT_MS;
}

static const stSdcardInterface *sdcardPortGetInterface(const stSdcardCfg *cfg)
{
    (void)cfg;
    return NULL;
}

static bool sdcardPortIsValidCfg(const stSdcardCfg *cfg)
{
    (void)cfg;
    return false;
}

static const stSdcardOps gSdcardPortOps = {
    .loadDefaultCfg = sdcardPortLoadDefaultCfg,
    .getInterface = sdcardPortGetInterface,
    .isValidCfg = sdcardPortIsValidCfg,
};

const stSdcardOps *sdcardPortGetOps(void)
{
    return &gSdcardPortOps;
}

/**************************End of file********************************/