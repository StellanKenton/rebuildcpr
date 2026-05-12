/***********************************************************************************
* @file     : drvgpio_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio_port.h"

#include "drvgpio.h"

#include "../bsp/bspgpio.h"

static const stDrvGpioBspInterface gDrvGpioBspInterface = {
    .init = bspGpioInit,
    .write = bspGpioWrite,
    .read = bspGpioRead,
    .toggle = bspGpioToggle,
};

static const stDrvGpioBspInterface *drvGpioPortGetBspInterfaceImpl(void)
{
    return &gDrvGpioBspInterface;
}

static const stDrvGpioOps gDrvGpioOps = {
    .getBspInterface = drvGpioPortGetBspInterfaceImpl,
};

const stDrvGpioOps *drvGpioPortGetOps(void)
{
    return &gDrvGpioOps;
}

/**************************End of file********************************/
