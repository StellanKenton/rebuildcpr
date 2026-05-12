/************************************************************************************
* @file     : ringbuffer_port.h
* @brief    : Project-side ringbuffer binding entry.
* @details  : Exposes the optional synchronization hooks used by the reusable
*             ringbuffer core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_RINGBUFFER_PORT_H
#define REBUILDCPR_RINGBUFFER_PORT_H

#include "../../rep/tools/ringbuffer/ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

const stRingBufferOps *ringBufferPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_RINGBUFFER_PORT_H
/**************************End of file********************************/