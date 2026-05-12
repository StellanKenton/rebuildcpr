/************************************************************************************
* @file     : frameparser_port.h
* @brief    : Project-side frameparser binding entry.
* @details  : Exposes protocol providers and timing hooks for the reusable
*             frameparser core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_FRAMEPARSER_PORT_H
#define REBUILDCPR_FRAMEPARSER_PORT_H

#include "framepareser.h"

#ifdef __cplusplus
extern "C" {
#endif

const stFrmPsrOps *frmPsrPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_FRAMEPARSER_PORT_H
/**************************End of file********************************/