/***********************************************************************************
* @file     : drviic_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drviic_port.h"

#include <stddef.h>
#include <string.h>

#include "i2c.h"
#include "drviic.h"

static eDrvStatus drvIicPortStatusFromHal(HAL_StatusTypeDef halStatus)
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

static I2C_HandleTypeDef *drvIicPortGetHandle(uint8_t iic)
{
    switch ((eDrvIicPortMap)iic) {
        case DRVIIC_BUS0:
            return &hi2c1;
        case DRVIIC_BUS1:
            return &hi2c2;
        default:
            return NULL;
    }
}

static eDrvStatus drvIicPortInit(uint8_t iic)
{
    I2C_HandleTypeDef *handle = drvIicPortGetHandle(iic);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvIicPortRecoverBus(uint8_t iic)
{
    I2C_HandleTypeDef *handle = drvIicPortGetHandle(iic);
    HAL_StatusTypeDef halStatus;

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    halStatus = HAL_I2C_DeInit(handle);
    if (halStatus != HAL_OK) {
        return drvIicPortStatusFromHal(halStatus);
    }

    halStatus = HAL_I2C_Init(handle);
    return drvIicPortStatusFromHal(halStatus);
}

static eDrvStatus drvIicPortTransfer(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)
{
    I2C_HandleTypeDef *handle = drvIicPortGetHandle(iic);
    uint16_t address;
    HAL_StatusTypeDef halStatus;

    if ((handle == NULL) || (transfer == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    address = (uint16_t)(transfer->address << 1U);

    if ((transfer->readLength > 0U) && (transfer->writeLength > 0U) && (transfer->secondWriteLength == 0U)) {
        uint16_t memAddSize = I2C_MEMADD_SIZE_8BIT;
        uint16_t memAddress = transfer->writeBuffer[0];

        if ((transfer->writeBuffer == NULL) || (transfer->writeLength > 2U)) {
            return DRV_STATUS_INVALID_PARAM;
        }

        if (transfer->writeLength == 2U) {
            memAddSize = I2C_MEMADD_SIZE_16BIT;
            memAddress = (uint16_t)(((uint16_t)transfer->writeBuffer[0] << 8U) | transfer->writeBuffer[1]);
        }

        halStatus = HAL_I2C_Mem_Read(handle,
                                     address,
                                     memAddress,
                                     memAddSize,
                                     transfer->readBuffer,
                                     transfer->readLength,
                                     timeoutMs);
        return drvIicPortStatusFromHal(halStatus);
    }

    if (transfer->secondWriteLength > 0U) {
        uint8_t buffer[64];
        uint16_t totalLength = (uint16_t)(transfer->writeLength + transfer->secondWriteLength);

        if ((transfer->writeBuffer == NULL) || (transfer->secondWriteBuffer == NULL) || (totalLength > sizeof(buffer))) {
            return DRV_STATUS_INVALID_PARAM;
        }

        (void)memcpy(buffer, transfer->writeBuffer, transfer->writeLength);
        (void)memcpy(&buffer[transfer->writeLength], transfer->secondWriteBuffer, transfer->secondWriteLength);
        halStatus = HAL_I2C_Master_Transmit(handle, address, buffer, totalLength, timeoutMs);
        return drvIicPortStatusFromHal(halStatus);
    }

    if (transfer->writeLength > 0U) {
        halStatus = HAL_I2C_Master_Transmit(handle,
                                            address,
                                            (uint8_t *)transfer->writeBuffer,
                                            transfer->writeLength,
                                            timeoutMs);
        if ((halStatus != HAL_OK) || (transfer->readLength == 0U)) {
            return drvIicPortStatusFromHal(halStatus);
        }
    }

    if (transfer->readLength > 0U) {
        halStatus = HAL_I2C_Master_Receive(handle,
                                           address,
                                           transfer->readBuffer,
                                           transfer->readLength,
                                           timeoutMs);
        return drvIicPortStatusFromHal(halStatus);
    }

    return DRV_STATUS_OK;
}

static const stDrvIicBspInterface gDrvIicBspInterfaces[DRVIIC_MAX] = {
    [DRVIIC_BUS0] = {
        .init = drvIicPortInit,
        .transfer = drvIicPortTransfer,
        .recoverBus = drvIicPortRecoverBus,
        .defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
    },
    [DRVIIC_BUS1] = {
        .init = drvIicPortInit,
        .transfer = drvIicPortTransfer,
        .recoverBus = drvIicPortRecoverBus,
        .defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
    },
};

const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void)
{
    return gDrvIicBspInterfaces;
}

/**************************End of file********************************/