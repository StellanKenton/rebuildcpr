/***********************************************************************************
* @file     : log_port.c
* @brief    : Project-side log transport binding.
* @details  : Binds the reusable log layer to the board RTT transport.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "log_port.h"

#include "../bsp/bsp_rtt.h"

static const stLogInterface gLogInterfaces[] = {
    {
        .transport = LOG_TRANSPORT_RTT,
        .init = bspRttLogInit,
        .write = bspRttLogWrite,
        .getBuffer = bspRttLogGetInputBuffer,
        .isOutputEnabled = (BSP_RTT_LOG_OUTPUT_ENABLE != 0),
        .isInputEnabled = (BSP_RTT_LOG_INPUT_ENABLE != 0),
    },
};

static void logPortConsolePollImpl(void);

static const stLogOps gLogOps = {
    .interfaces = gLogInterfaces,
    .interfaceCount = (uint32_t)(sizeof(gLogInterfaces) / sizeof(gLogInterfaces[0])),
    .consolePoll = logPortConsolePollImpl,
};

static void logPortConsolePollImpl(void)
{
    (void)bspRttLogGetInputBuffer();
}

const stLogOps *logPortGetOps(void)
{
    return &gLogOps;
}

/**************************End of file********************************/
