/************************************************************************************
* @file     : trace_port.h
* @brief    : Project-side trace binding entry.
* @details  : Exposes the transport and halt hooks used by the reusable trace
*             core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_TRACE_PORT_H
#define REBUILDCPR_TRACE_PORT_H

#include "../../rep/tools/trace/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

const stTraceOps *tracePortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_TRACE_PORT_H
/**************************End of file********************************/