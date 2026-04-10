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

#include <stddef.h>
#include <string.h>

#include "main.h"
#include "spi.h"
#include "drvspi.h"

typedef struct stDrvSpiCsPin {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    bool isActiveLow;
} stDrvSpiCsPin;

static eDrvStatus drvSpiPortStatusFromHal(HAL_StatusTypeDef halStatus)
{
    switch (halStatus) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        default:
            return DRV_STATUS_ERROR;
    }
}

static SPI_HandleTypeDef *drvSpiPortGetHandle(uint8_t spi)
{
    if (spi != DRVSPI_BUS0) {
        return NULL;
    }

    return &hspi1;
}

static eDrvStatus drvSpiPortInit(uint8_t spi)
{
    SPI_HandleTypeDef *handle = drvSpiPortGetHandle(spi);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvSpiPortTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
    SPI_HandleTypeDef *handle = drvSpiPortGetHandle(spi);
    HAL_StatusTypeDef halStatus;
    uint8_t fillBuffer[32];
    uint16_t chunkLength;
    uint16_t offset;

    if ((handle == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if ((txBuffer != NULL) && (rxBuffer != NULL)) {
        halStatus = HAL_SPI_TransmitReceive(handle, (uint8_t *)txBuffer, rxBuffer, length, timeoutMs);
        return drvSpiPortStatusFromHal(halStatus);
    }

    if (txBuffer != NULL) {
        halStatus = HAL_SPI_Transmit(handle, (uint8_t *)txBuffer, length, timeoutMs);
        return drvSpiPortStatusFromHal(halStatus);
    }

    if (rxBuffer == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    (void)memset(fillBuffer, fillData, sizeof(fillBuffer));
    offset = 0U;
    while (offset < length) {
        chunkLength = (uint16_t)(length - offset);
        if (chunkLength > sizeof(fillBuffer)) {
            chunkLength = sizeof(fillBuffer);
        }

        halStatus = HAL_SPI_TransmitReceive(handle,
                                            fillBuffer,
                                            &rxBuffer[offset],
                                            chunkLength,
                                            timeoutMs);
        if (halStatus != HAL_OK) {
            return drvSpiPortStatusFromHal(halStatus);
        }

        offset = (uint16_t)(offset + chunkLength);
    }

    return DRV_STATUS_OK;
}

static void drvSpiPortCsInit(void *context)
{
    stDrvSpiCsPin *pin = (stDrvSpiCsPin *)context;

    if (pin == NULL) {
        return;
    }

    HAL_GPIO_WritePin(pin->gpioPort,
                      pin->gpioPin,
                      pin->isActiveLow ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void drvSpiPortCsWrite(void *context, bool isActive)
{
    stDrvSpiCsPin *pin = (stDrvSpiCsPin *)context;
    GPIO_PinState state;

    if (pin == NULL) {
        return;
    }

    state = (isActive == pin->isActiveLow) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(pin->gpioPort, pin->gpioPin, state);
}

static stDrvSpiCsPin gDrvSpiBus0CsPin = {
    .gpioPort = SPI_CS_GPIO_Port,
    .gpioPin = SPI_CS_Pin,
    .isActiveLow = true,
};

static const stDrvSpiBspInterface gDrvSpiBspInterfaces[DRVSPI_MAX] = {
    [DRVSPI_BUS0] = {
        .init = drvSpiPortInit,
        .transfer = drvSpiPortTransfer,
        .defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
        .csControl = {
            .init = drvSpiPortCsInit,
            .write = drvSpiPortCsWrite,
            .context = &gDrvSpiBus0CsPin,
        },
    },
};

const stDrvSpiBspInterface *drvSpiGetPlatformBspInterfaces(void)
{
    return gDrvSpiBspInterfaces;
}

/**************************End of file********************************/
