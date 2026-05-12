/***********************************************************************************
* @file     : drvspi_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvspi_port.h"

#include "drvspi.h"

#include "../bsp/bspspi.h"

const stDrvSpiBspInterface gDrvSpiBspInterface[DRVSPI_MAX] = {
    [DRVSPI_BUS0] = {
        .init = bspSpiInit,
        .transfer = bspSpiTransfer,
        .defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
        .csControl = {
            .init = bspSpiCsInit,
            .write = bspSpiCsWrite,
            .context = (void *)&gBspSpiBus0CsPin,
        },
    },
};

static const stDrvSpiBspInterface *drvSpiPortGetBspInterfacesImpl(void)
{
    return gDrvSpiBspInterface;
}

static const stDrvSpiOps gDrvSpiOps = {
    .getBspInterfaces = drvSpiPortGetBspInterfacesImpl,
};

const stDrvSpiOps *drvSpiPortGetOps(void)
{
    return &gDrvSpiOps;
}

/**************************End of file********************************/
