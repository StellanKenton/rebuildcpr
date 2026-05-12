/************************************************************************************
* @file     : sdcard_port.h
* @brief    : Project-side SD card binding entry.
* @details  : Exposes the explicit ops table consumed by the reusable SD card
*             module core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SDCARD_PORT_H
#define REBUILDCPR_SDCARD_PORT_H

#include "../../rep/module/sdcard/sdcard_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

const stSdcardOps *sdcardPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_SDCARD_PORT_H
/**************************End of file********************************/