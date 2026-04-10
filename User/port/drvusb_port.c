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

#include <stdbool.h>
#include <stddef.h>

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "drvusb.h"
#include "ringbuffer.h"
#include "rtos.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static stRingBuffer gDrvUsbRxRingBuffer[DRVUSB_MAX];
static uint8_t gDrvUsbRxStorageFs[DRVUSB_PORT_RX_STORAGE_FS];
static bool gDrvUsbInitialized[DRVUSB_MAX];
static bool gDrvUsbStarted[DRVUSB_MAX];

static eDrvStatus drvUsbPortStatusFromHal(HAL_StatusTypeDef halStatus)
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

static eDrvStatus drvUsbPortStatusFromUsbd(USBD_StatusTypeDef usbStatus)
{
    switch (usbStatus) {
        case USBD_OK:
            return DRV_STATUS_OK;
        case USBD_BUSY:
            return DRV_STATUS_BUSY;
        case USBD_FAIL:
        default:
            return DRV_STATUS_ERROR;
    }
}

static USBD_HandleTypeDef *drvUsbPortGetDevice(uint8_t usb)
{
    if (usb != DRVUSB_DEV0) {
        return NULL;
    }

    return &hUsbDeviceFS;
}

static PCD_HandleTypeDef *drvUsbPortGetPcd(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);

    if ((device == NULL) || (device->pData == NULL)) {
        return NULL;
    }

    return (PCD_HandleTypeDef *)device->pData;
}

static bool drvUsbPortIsCdcEndpoint(uint8_t endpointAddress)
{
    return (endpointAddress == DRVUSB_PORT_CDC_DATA_IN_EP) ||
           (endpointAddress == DRVUSB_PORT_CDC_DATA_OUT_EP) ||
           (endpointAddress == DRVUSB_PORT_CDC_CMD_EP);
}

static uint8_t drvUsbPortGetEndpointType(eDrvUsbEndpointType type)
{
    switch (type) {
        case DRVUSB_ENDPOINT_TYPE_CONTROL:
            return USBD_EP_TYPE_CTRL;
        case DRVUSB_ENDPOINT_TYPE_ISOCHRONOUS:
            return USBD_EP_TYPE_ISOC;
        case DRVUSB_ENDPOINT_TYPE_BULK:
            return USBD_EP_TYPE_BULK;
        case DRVUSB_ENDPOINT_TYPE_INTERRUPT:
        default:
            return USBD_EP_TYPE_INTR;
    }
}

static eDrvStatus drvUsbPortInit(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);

    if (device == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gDrvUsbInitialized[usb]) {
        return DRV_STATUS_OK;
    }

    if (ringBufferInit(&gDrvUsbRxRingBuffer[usb], gDrvUsbRxStorageFs, sizeof(gDrvUsbRxStorageFs)) != RINGBUFFER_OK) {
        return DRV_STATUS_ERROR;
    }

    MX_USB_DEVICE_Init();
    if (device->pData == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    gDrvUsbInitialized[usb] = true;
    gDrvUsbStarted[usb] = true;
    return DRV_STATUS_OK;
}

static eDrvStatus drvUsbPortStart(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);
    eDrvStatus status;

    if (device == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    status = drvUsbPortInit(usb);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    if (gDrvUsbStarted[usb]) {
        return DRV_STATUS_OK;
    }

    status = drvUsbPortStatusFromUsbd(USBD_Start(device));
    if (status == DRV_STATUS_OK) {
        gDrvUsbStarted[usb] = true;
    }
    return status;
}

static eDrvStatus drvUsbPortStop(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);
    eDrvStatus status;

    if ((device == NULL) || !gDrvUsbInitialized[usb]) {
        return DRV_STATUS_NOT_READY;
    }

    status = drvUsbPortStatusFromUsbd(USBD_Stop(device));
    if (status == DRV_STATUS_OK) {
        gDrvUsbStarted[usb] = false;
    }
    return status;
}

static eDrvStatus drvUsbPortSetConnect(uint8_t usb, bool isConnect)
{
    PCD_HandleTypeDef *pcd = drvUsbPortGetPcd(usb);

    if ((usb >= DRVUSB_MAX) || (pcd == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    if (isConnect) {
        return drvUsbPortStatusFromHal(HAL_PCD_DevConnect(pcd));
    }

    return drvUsbPortStatusFromHal(HAL_PCD_DevDisconnect(pcd));
}

static eDrvStatus drvUsbPortOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
    PCD_HandleTypeDef *pcd = drvUsbPortGetPcd(usb);

    if ((pcd == NULL) || (config == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvUsbPortIsCdcEndpoint(config->endpointAddress)) {
        return DRV_STATUS_OK;
    }

    return drvUsbPortStatusFromHal(HAL_PCD_EP_Open(pcd,
                                                    config->endpointAddress,
                                                    config->maxPacketSize,
                                                    drvUsbPortGetEndpointType(config->type)));
}

static eDrvStatus drvUsbPortCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    PCD_HandleTypeDef *pcd = drvUsbPortGetPcd(usb);

    if (pcd == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    if (drvUsbPortIsCdcEndpoint(endpointAddress)) {
        return DRV_STATUS_OK;
    }

    return drvUsbPortStatusFromHal(HAL_PCD_EP_Close(pcd, endpointAddress));
}

static eDrvStatus drvUsbPortFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    PCD_HandleTypeDef *pcd = drvUsbPortGetPcd(usb);

    if (pcd == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    return drvUsbPortStatusFromHal(HAL_PCD_EP_Flush(pcd, endpointAddress));
}

static eDrvStatus drvUsbPortTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint8_t usbStatus;

    if ((usb != DRVUSB_DEV0) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (endpointAddress != DRVUSB_PORT_CDC_DATA_IN_EP) {
        return DRV_STATUS_UNSUPPORTED;
    }

    if (!gDrvUsbStarted[usb] || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)) {
        return DRV_STATUS_NOT_READY;
    }

    startTick = HAL_GetTick();
    do {
        usbStatus = CDC_Transmit_FS((uint8_t *)buffer, length);
        if (usbStatus == USBD_OK) {
            return DRV_STATUS_OK;
        }

        if (usbStatus != USBD_BUSY) {
            return drvUsbPortStatusFromUsbd((USBD_StatusTypeDef)usbStatus);
        }

        (void)repRtosDelayMs(1U);
    } while ((HAL_GetTick() - startTick) < timeoutMs);

    return DRV_STATUS_TIMEOUT;
}

static eDrvStatus drvUsbPortReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint32_t availableLength;
    uint32_t readLength;

    if (actualLength != NULL) {
        *actualLength = 0U;
    }

    if ((usb != DRVUSB_DEV0) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (endpointAddress != DRVUSB_PORT_CDC_DATA_OUT_EP) {
        return DRV_STATUS_UNSUPPORTED;
    }

    if (!gDrvUsbInitialized[usb]) {
        return DRV_STATUS_NOT_READY;
    }

    startTick = HAL_GetTick();
    do {
        availableLength = ringBufferGetUsed(&gDrvUsbRxRingBuffer[usb]);
        if (availableLength > 0U) {
            readLength = availableLength;
            if (readLength > length) {
                readLength = length;
            }

            readLength = ringBufferRead(&gDrvUsbRxRingBuffer[usb], buffer, readLength);
            if (actualLength != NULL) {
                *actualLength = (uint16_t)readLength;
            }
            return DRV_STATUS_OK;
        }

        if (timeoutMs == 0U) {
            break;
        }

        (void)repRtosDelayMs(1U);
    } while ((HAL_GetTick() - startTick) < timeoutMs);

    return DRV_STATUS_TIMEOUT;
}

static bool drvUsbPortIsConnected(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);

    if ((device == NULL) || !gDrvUsbStarted[usb]) {
        return false;
    }

    return (device->dev_state == USBD_STATE_ADDRESSED) ||
           (device->dev_state == USBD_STATE_CONFIGURED) ||
           (device->dev_state == USBD_STATE_SUSPENDED);
}

static bool drvUsbPortIsConfigured(uint8_t usb)
{
    USBD_HandleTypeDef *device = drvUsbPortGetDevice(usb);

    return (device != NULL) && (device->dev_state == USBD_STATE_CONFIGURED);
}

static eDrvUsbSpeed drvUsbPortGetSpeed(uint8_t usb)
{
    PCD_HandleTypeDef *pcd = drvUsbPortGetPcd(usb);

    if (pcd == NULL) {
        return DRVUSB_SPEED_UNKNOWN;
    }

    switch (pcd->Init.speed) {
        case PCD_SPEED_FULL:
            return DRVUSB_SPEED_FULL;
        default:
            return DRVUSB_SPEED_UNKNOWN;
    }
}

void usbdCdcIfUserReceiveCallback(const uint8_t *buffer, uint32_t length)
{
    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    (void)ringBufferWrite(&gDrvUsbRxRingBuffer[DRVUSB_DEV0], buffer, length);
}

static const stDrvUsbBspInterface gDrvUsbBspInterfaces[DRVUSB_MAX] = {
    [DRVUSB_DEV0] = {
        .init = drvUsbPortInit,
        .start = drvUsbPortStart,
        .stop = drvUsbPortStop,
        .setConnect = drvUsbPortSetConnect,
        .openEndpoint = drvUsbPortOpenEndpoint,
        .closeEndpoint = drvUsbPortCloseEndpoint,
        .flushEndpoint = drvUsbPortFlushEndpoint,
        .transmit = drvUsbPortTransmit,
        .receive = drvUsbPortReceive,
        .isConnected = drvUsbPortIsConnected,
        .isConfigured = drvUsbPortIsConfigured,
        .getSpeed = drvUsbPortGetSpeed,
        .defaultTimeoutMs = DRVUSB_DEFAULT_TIMEOUT_MS,
        .role = DRVUSB_ROLE_DEVICE,
    },
};

const stDrvUsbBspInterface *drvUsbGetPlatformBspInterfaces(void)
{
    return gDrvUsbBspInterfaces;
}

/**************************End of file********************************/
