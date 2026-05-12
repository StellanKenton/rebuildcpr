/************************************************************************************
* @file     : drvgpio_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVGPIO_PORT_H
#define REBUILDCPR_DRVGPIO_PORT_H

#include "drvgpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinMap {
    DRVGPIO_EN_AUDIO = 0,
    DRVGPIO_RESET_WIFI,
    DRVGPIO_USB_SELECT,
    DRVGPIO_POWER_ON_CHECK,
    DRVGPIO_BAT_CHARGING_STATUS,
    DRVGPIO_BAT_CHARGE_DONE_STATUS,
    DRVGPIO_PCA9535_SCL,
    DRVGPIO_PCA9535_SDA,
    DRVGPIO_PCA9535_RESET,
    DRVGPIO_TM1651_CLK,
    DRVGPIO_TM1651_SDA,
    DRVGPIO_SPI_CS,
    DRVGPIO_POWER_ON_CTRL,
} eDrvGpioPinMap;

const stDrvGpioOps *drvGpioPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
