/************************************************************************************
* @file     : wt2003hx_port.h
* @brief    : Project-side WT2003HX binding helpers.
* @details  : Binds the reusable WT2003HX module to the current board UART and
*             enable GPIO.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WT2003HX_PORT_H
#define REBUILDCPR_WT2003HX_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "wt2003hx.h"
#include "wt2003hx_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef stWt2003hxTransportInterface stWt2003hxPortTransportInterface;
typedef stWt2003hxControlInterface stWt2003hxPortControlInterface;

const stWt2003hxOps *wt2003hxPortGetOps(void);

eDrvStatus wt2003hxPortInit(void);
eDrvStatus wt2003hxPortProcess(void);
bool wt2003hxPortIsReady(void);
eDrvStatus wt2003hxPortPlayName(const uint8_t *name, uint8_t nameLen);
eDrvStatus wt2003hxPortStop(void);
eDrvStatus wt2003hxPortSetVolume(uint8_t volume);
eDrvStatus wt2003hxPortSetPlayMode(eWt2003hxPlayMode mode);
eDrvStatus wt2003hxPortSetOutputMode(eWt2003hxOutputMode mode);
eDrvStatus wt2003hxPortQuery(uint8_t cmd);
bool wt2003hxPortGetInfo(stWt2003hxInfo *info);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_WT2003HX_PORT_H
/**************************End of file********************************/
