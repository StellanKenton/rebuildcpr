/************************************************************************************
* @file     : drvmcuflash_port.h
* @brief    : Project-side MCU flash binding entry.
* @details  : Exposes the explicit provider used by the reusable drvmcuflash core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVMCUFLASH_PORT_H
#define REBUILDCPR_DRVMCUFLASH_PORT_H

#include "../../rep/driver/drvmcuflash/drvmcuflash.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVMCUFLASH_PORT_AREA_START
#define DRVMCUFLASH_PORT_AREA_START          0x00000000UL
#endif

#ifndef DRVMCUFLASH_PORT_AREA_SIZE
#define DRVMCUFLASH_PORT_AREA_SIZE           0x00000000UL
#endif

typedef enum eDrvMcuFlashAreaMap {
    DRVMCUFLASH_AREA_USER = 0,
    DRVMCUFLASH_AREA_MAX,
} eDrvMcuFlashAreaMap;

const stDrvMcuFlashOps *drvMcuFlashPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_DRVMCUFLASH_PORT_H
/**************************End of file********************************/