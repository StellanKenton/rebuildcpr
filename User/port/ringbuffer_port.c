/***********************************************************************************
* @file     : ringbuffer_port.c
* @brief    : Project-side ringbuffer binding implementation.
* @details  : Keeps the default ringbuffer hooks empty so the reusable core stays
*             transport-agnostic while still using an explicit ops entry point.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "ringbuffer_port.h"

#include <stddef.h>

static const stRingBufferOps gRingBufferOps = {
    .enterCritical = NULL,
    .exitCritical = NULL,
    .memoryBarrier = NULL,
};

const stRingBufferOps *ringBufferPortGetOps(void)
{
    return &gRingBufferOps;
}

/**************************End of file********************************/