/***********************************************************************************
* @file     : drvuart_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvuart_port.h"

#include <stddef.h>

#include "usart.h"
#include "drvuart.h"

static stRingBuffer gDrvUartRingBuffer[DRVUART_MAX];
static uint8_t gDrvUartRxStorageDebug[DRVUART_RECVLEN_DEBUGUART];
static uint8_t gDrvUartRxStorageAudio[DRVUART_RECVLEN_AUDIO];

static eDrvStatus drvUartPortStatusFromHal(HAL_StatusTypeDef halStatus)
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

static UART_HandleTypeDef *drvUartPortGetHandle(uint8_t uart)
{
    switch ((eDrvUartPortMap)uart) {
        case DRVUART_DEBUG:
            return &huart4;
        case DRVUART_AUDIO:
            return &huart2;
        default:
            return NULL;
    }
}

static eDrvStatus drvUartPortInit(uint8_t uart)
{
    UART_HandleTypeDef *handle = drvUartPortGetHandle(uart);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvUartPortTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    UART_HandleTypeDef *handle = drvUartPortGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartPortStatusFromHal(HAL_UART_Transmit(handle, (uint8_t *)buffer, length, timeoutMs));
}

static eDrvStatus drvUartPortTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = drvUartPortGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartPortStatusFromHal(HAL_UART_Transmit_IT(handle, (uint8_t *)buffer, length));
}

static eDrvStatus drvUartPortTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = drvUartPortGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartPortStatusFromHal(HAL_UART_Transmit_DMA(handle, (uint8_t *)buffer, length));
}

static uint16_t drvUartPortGetDataLen(uint8_t uart)
{
    (void)uart;
    return 0U;
}

static eDrvStatus drvUartPortReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
    UART_HandleTypeDef *handle = drvUartPortGetHandle(uart);

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartPortStatusFromHal(HAL_UART_Receive(handle, buffer, length, 0U));
}

static stDrvUartBspInterface gDrvUartBspInterface[DRVUART_MAX] = {
    [DRVUART_DEBUG] = {
        .init = drvUartPortInit,
        .transmit = drvUartPortTransmit,
        .transmitIt = drvUartPortTransmitIt,
        .transmitDma = drvUartPortTransmitDma,
        .getDataLen = drvUartPortGetDataLen,
        .receive = drvUartPortReceive,
        .Buffer = gDrvUartRxStorageDebug,
    },
    [DRVUART_AUDIO] = {
        .init = drvUartPortInit,
        .transmit = drvUartPortTransmit,
        .transmitIt = drvUartPortTransmitIt,
        .transmitDma = drvUartPortTransmitDma,
        .getDataLen = drvUartPortGetDataLen,
        .receive = drvUartPortReceive,
        .Buffer = gDrvUartRxStorageAudio,
    },
};

const stDrvUartBspInterface *drvUartGetPlatformBspInterfaces(void)
{
    return gDrvUartBspInterface;
}

stRingBuffer *drvUartGetPlatformRingBuffer(uint8_t uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    return &gDrvUartRingBuffer[uart];
}

eDrvStatus drvUartGetPlatformStorageConfig(uint8_t uart, uint8_t **storage, uint32_t *capacity)
{
    if ((storage == NULL) || (capacity == NULL) || (uart >= DRVUART_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    switch ((eDrvUartPortMap)uart) {
        case DRVUART_DEBUG:
            *storage = gDrvUartRxStorageDebug;
            *capacity = sizeof(gDrvUartRxStorageDebug);
            return DRV_STATUS_OK;
        case DRVUART_AUDIO:
            *storage = gDrvUartRxStorageAudio;
            *capacity = sizeof(gDrvUartRxStorageAudio);
            return DRV_STATUS_OK;
        default:
            *storage = NULL;
            *capacity = 0U;
            return DRV_STATUS_UNSUPPORTED;
    }
}

/**************************End of file********************************/