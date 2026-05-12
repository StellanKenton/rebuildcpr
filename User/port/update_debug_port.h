/************************************************************************************
* @file     : update_debug_port.h
* @brief    : Project-side update debug binding entry.
* @details  : Exposes the optional update debug hooks consumed by the reusable
*             update debug helpers.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_UPDATE_DEBUG_PORT_H
#define REBUILDCPR_UPDATE_DEBUG_PORT_H

#include "../../rep/sys/update/update_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

const stUpdateDbgOps *updateDebugPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_UPDATE_DEBUG_PORT_H
/**************************End of file********************************/