/***********************************************************************************
* @file     : drvusb_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvusb_port.h"

#include "drvusb.h"

#include "../bsp/bspusb.h"

static const stDrvUsbBspInterface gDrvUsbBspInterfaces[DRVUSB_MAX] = {
    [DRVUSB_DEV0] = {
        .init = bspUsbInit,
        .start = bspUsbStart,
        .stop = bspUsbStop,
        .setConnect = bspUsbSetConnect,
        .openEndpoint = bspUsbOpenEndpoint,
        .closeEndpoint = bspUsbCloseEndpoint,
        .flushEndpoint = bspUsbFlushEndpoint,
        .transmit = bspUsbTransmit,
        .receive = bspUsbReceive,
        .isConnected = bspUsbIsConnected,
        .isConfigured = bspUsbIsConfigured,
        .getSpeed = bspUsbGetSpeed,
        .defaultTimeoutMs = DRVUSB_DEFAULT_TIMEOUT_MS,
        .role = DRVUSB_ROLE_DEVICE,
    },
};

static const stDrvUsbBspInterface *drvUsbPortGetBspInterfacesImpl(void)
{
    return gDrvUsbBspInterfaces;
}

static const stDrvUsbOps gDrvUsbOps = {
    .getBspInterfaces = drvUsbPortGetBspInterfacesImpl,
};

const stDrvUsbOps *drvUsbPortGetOps(void)
{
    return &gDrvUsbOps;
}

/**************************End of file********************************/
