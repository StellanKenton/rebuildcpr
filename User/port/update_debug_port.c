/***********************************************************************************
* @file     : update_debug_port.c
* @brief    : Project-side update debug binding implementation.
* @details  : Leaves update debug callbacks disabled until the current product
*             needs explicit transition tracing.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_debug_port.h"

static const stUpdateDbgOps gUpdateDbgOps = {
    .onStateChanged = NULL,
};

const stUpdateDbgOps *updateDebugPortGetOps(void)
{
    return &gUpdateDbgOps;
}

/**************************End of file********************************/