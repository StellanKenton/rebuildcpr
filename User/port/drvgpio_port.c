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

#include <stdbool.h>

#include "main.h"
#include "drvgpio.h"

typedef struct stDrvGpioMap {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    bool isOutput;
} stDrvGpioMap;

static const stDrvGpioMap gDrvGpioMap[DRVGPIO_MAX] = {
    [DRVGPIO_EN_AUDIO] = {
        .gpioPort = EN_AUDIO_GPIO_Port,
        .gpioPin = EN_AUDIO_Pin,
        .isOutput = true,
    },
    [DRVGPIO_RESET_WIFI] = {
        .gpioPort = RESET_WIFI_GPIO_Port,
        .gpioPin = RESET_WIFI_Pin,
        .isOutput = true,
    },
    [DRVGPIO_USB_SELECT] = {
        .gpioPort = USB_Select_GPIO_Port,
        .gpioPin = USB_Select_Pin,
        .isOutput = true,
    },
    [DRVGPIO_POWER_ON_CHECK] = {
        .gpioPort = Power_ON_Check_GPIO_Port,
        .gpioPin = Power_ON_Check_Pin,
        .isOutput = false,
    },
    [DRVGPIO_BAT_CHARGING_STATUS] = {
        .gpioPort = BAT_Charging_Status_GPIO_Port,
        .gpioPin = BAT_Charging_Status_Pin,
        .isOutput = false,
    },
    [DRVGPIO_BAT_CHARGE_DONE_STATUS] = {
        .gpioPort = BAT_ChargeDone_Status_GPIO_Port,
        .gpioPin = BAT_ChargeDone_Status_Pin,
        .isOutput = false,
    },
};

static void drvGpioPortInit(void)
{
    HAL_GPIO_WritePin(EN_AUDIO_GPIO_Port, EN_AUDIO_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(USB_Select_GPIO_Port, USB_Select_Pin, GPIO_PIN_RESET);
}

static void drvGpioPortWrite(uint8_t pin, eDrvGpioPinState state)
{
    if ((pin >= DRVGPIO_MAX) || !gDrvGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_WritePin(gDrvGpioMap[pin].gpioPort,
                      gDrvGpioMap[pin].gpioPin,
                      (state == DRVGPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static eDrvGpioPinState drvGpioPortRead(uint8_t pin)
{
    if (pin >= DRVGPIO_MAX) {
        return DRVGPIO_PIN_STATE_INVALID;
    }

    return (HAL_GPIO_ReadPin(gDrvGpioMap[pin].gpioPort, gDrvGpioMap[pin].gpioPin) == GPIO_PIN_SET) ?
        DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET;
}

static void drvGpioPortToggle(uint8_t pin)
{
    if ((pin >= DRVGPIO_MAX) || !gDrvGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_TogglePin(gDrvGpioMap[pin].gpioPort, gDrvGpioMap[pin].gpioPin);
}

static const stDrvGpioBspInterface gDrvGpioBspInterface = {
    .init = drvGpioPortInit,
    .write = drvGpioPortWrite,
    .read = drvGpioPortRead,
    .toggle = drvGpioPortToggle,
};

const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void)
{
    return &gDrvGpioBspInterface;
}

/**************************End of file********************************/