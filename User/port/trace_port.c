/***********************************************************************************
* @file     : trace_port.c
* @brief    : Project-side trace binding implementation.
* @details  : Routes optional trace output through the existing RTT transport.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "trace_port.h"

#include "../bsp/bsp_rtt.h"

static const stTraceOps gTraceOps = {
    .transportInit = bspRttLogInit,
    .transportWrite = bspRttLogWrite,
    .halt = NULL,
};

const stTraceOps *tracePortGetOps(void)
{
    return &gTraceOps;
}

/**************************End of file********************************/