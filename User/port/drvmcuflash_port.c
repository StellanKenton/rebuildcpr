/***********************************************************************************
* @file     : drvmcuflash_port.c
* @brief    : Project-side MCU flash binding implementation.
* @details  : Provides an explicit ops table for drvmcuflash. The current product
*             leaves MCU flash write support unbound until a project area map and
*             BSP implementation are provided.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvmcuflash_port.h"

#include <stddef.h>

static const stDrvMcuFlashBspInterface *drvMcuFlashPortGetBspInterfaceImpl(void);
static eDrvStatus drvMcuFlashPortGetAreaInfoImpl(uint8_t area, stDrvMcuFlashAreaInfo *info);
static uint8_t drvMcuFlashPortGetAreaCountImpl(void);

static const stDrvMcuFlashOps gDrvMcuFlashOps = {
    .getBspInterface = drvMcuFlashPortGetBspInterfaceImpl,
    .getAreaInfo = drvMcuFlashPortGetAreaInfoImpl,
    .getAreaCount = drvMcuFlashPortGetAreaCountImpl,
};

static const stDrvMcuFlashBspInterface *drvMcuFlashPortGetBspInterfaceImpl(void)
{
    return NULL;
}

static eDrvStatus drvMcuFlashPortGetAreaInfoImpl(uint8_t area, stDrvMcuFlashAreaInfo *info)
{
    (void)area;
    (void)info;
    return DRV_STATUS_UNSUPPORTED;
}

static uint8_t drvMcuFlashPortGetAreaCountImpl(void)
{
    return 0U;
}

const stDrvMcuFlashOps *drvMcuFlashPortGetOps(void)
{
    return &gDrvMcuFlashOps;
}

/**************************End of file********************************/