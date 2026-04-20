/***********************************************************************************
* @file     : frameprocess.c
* @brief    : Project-side USB communication manager implementation.
* @details  : Uses the reusable drvusb abstraction to expose a simple CDC-backed
*             communication path for the current product.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "frameprocess.h"

#include <string.h>

#include "drvusb.h"
#include "ringbuffer.h"
#include "rtos.h"

#include "../../../../rep/service/log/log.h"
#include "../../../port/drvusb_port.h"

typedef struct stFrmProcContext {
    bool isInitialized;
    stFrmProcInfo info;
    stRingBuffer rxRingBuffer;
    uint8_t rxStorage[FRM_PROC_RX_BUFFER_SIZE];
    uint8_t ioBuffer[FRM_PROC_IO_CHUNK_SIZE];
} stFrmProcContext;

static stFrmProcContext gFrmProcContext[FRAME_PROC_COUNT];

static bool frmProcIsValid(uint8_t process)
{
    return process < FRAME_PROC_COUNT;
}

static eFrmProcStatus frmProcMapDrvStatus(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return FRM_PROC_STATUS_OK;
        case DRV_STATUS_INVALID_PARAM:
            return FRM_PROC_STATUS_INVALID_PARAM;
        case DRV_STATUS_NOT_READY:
        case DRV_STATUS_UNSUPPORTED:
            return FRM_PROC_STATUS_NOT_READY;
        case DRV_STATUS_BUSY:
            return FRM_PROC_STATUS_BUSY;
        case DRV_STATUS_TIMEOUT:
            return FRM_PROC_STATUS_TIMEOUT;
        default:
            return FRM_PROC_STATUS_ERROR;
    }
}

static stFrmProcContext *frmProcGetContext(uint8_t process)
{
    if (!frmProcIsValid(process)) {
        return NULL;
    }

    return &gFrmProcContext[process];
}

static void frmProcRefreshState(stFrmProcContext *context)
{
    if ((context == NULL) || !context->isInitialized) {
        return;
    }

    context->info.isConnected = drvUsbIsConnected(DRVUSB_DEV0);
    context->info.isConfigured = drvUsbIsConfigured(DRVUSB_DEV0);

    if (context->info.isConfigured) {
        context->info.state = eFRM_PROC_STATE_RUNNING;
    } else if (context->info.isConnected) {
        context->info.state = eFRM_PROC_STATE_READY;
    } else {
        context->info.state = eFRM_PROC_STATE_READY;
    }
}

eFrmProcStatus frmProcInit(uint8_t process)
{
    stFrmProcContext *context;
    eDrvStatus lDrvStatus;

    context = frmProcGetContext(process);
    if (context == NULL) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (context->isInitialized) {
        frmProcRefreshState(context);
        return FRM_PROC_STATUS_OK;
    }

    (void)memset(context, 0, sizeof(*context));
    if (ringBufferInit(&context->rxRingBuffer, context->rxStorage, sizeof(context->rxStorage)) != RINGBUFFER_OK) {
        context->info.state = eFRM_PROC_STATE_ERROR;
        return FRM_PROC_STATUS_ERROR;
    }

    lDrvStatus = drvUsbInit(DRVUSB_DEV0);
    if (lDrvStatus != DRV_STATUS_OK) {
        context->info.state = eFRM_PROC_STATE_ERROR;
        LOG_E(FRM_PROC_LOG_TAG, "usb init fail status=%d", (int)lDrvStatus);
        return frmProcMapDrvStatus(lDrvStatus);
    }

    lDrvStatus = drvUsbStart(DRVUSB_DEV0);
    if (lDrvStatus != DRV_STATUS_OK) {
        context->info.state = eFRM_PROC_STATE_ERROR;
        LOG_E(FRM_PROC_LOG_TAG, "usb start fail status=%d", (int)lDrvStatus);
        return frmProcMapDrvStatus(lDrvStatus);
    }

    lDrvStatus = drvUsbDisconnect(DRVUSB_DEV0);
    if ((lDrvStatus != DRV_STATUS_OK) && (lDrvStatus != DRV_STATUS_UNSUPPORTED)) {
        context->info.state = eFRM_PROC_STATE_ERROR;
        LOG_E(FRM_PROC_LOG_TAG, "usb disconnect fail status=%d", (int)lDrvStatus);
        return frmProcMapDrvStatus(lDrvStatus);
    }

    (void)repRtosDelayMs(20U);

    lDrvStatus = drvUsbConnect(DRVUSB_DEV0);
    if ((lDrvStatus != DRV_STATUS_OK) && (lDrvStatus != DRV_STATUS_UNSUPPORTED)) {
        context->info.state = eFRM_PROC_STATE_ERROR;
        LOG_E(FRM_PROC_LOG_TAG, "usb connect fail status=%d", (int)lDrvStatus);
        return frmProcMapDrvStatus(lDrvStatus);
    }

    context->isInitialized = true;
    context->info.state = eFRM_PROC_STATE_READY;
    frmProcRefreshState(context);
    LOG_I(FRM_PROC_LOG_TAG, "usb comm init ok");
    return FRM_PROC_STATUS_OK;
}

void frmProcProcess(uint8_t process)
{
    stFrmProcContext *context;

    context = frmProcGetContext(process);
    if ((context == NULL) || !context->isInitialized) {
        return;
    }

    frmProcRefreshState(context);
    while (context->info.isConnected) {
        uint16_t lActualLength = 0U;
        uint32_t lWrittenLength;
        eDrvStatus lDrvStatus;

        lDrvStatus = drvUsbReceiveTimeout(DRVUSB_DEV0,
                                          DRVUSB_PORT_CDC_DATA_OUT_EP,
                                          context->ioBuffer,
                                          (uint16_t)sizeof(context->ioBuffer),
                                          &lActualLength,
                                          0U);
        if ((lDrvStatus != DRV_STATUS_OK) || (lActualLength == 0U)) {
            if ((lDrvStatus != DRV_STATUS_TIMEOUT) && (lDrvStatus != DRV_STATUS_NOT_READY)) {
                context->info.state = eFRM_PROC_STATE_ERROR;
            }
            break;
        }

        lWrittenLength = ringBufferWrite(&context->rxRingBuffer, context->ioBuffer, lActualLength);
        context->info.rxBytes += lActualLength;
        if (lWrittenLength < lActualLength) {
            context->info.dropBytes += (uint32_t)(lActualLength - lWrittenLength);
            LOG_W(FRM_PROC_LOG_TAG,
                  "usb rx drop=%u",
                  (unsigned int)(lActualLength - lWrittenLength));
        }

        frmProcRefreshState(context);
    }
}

eFrmProcStatus frmProcSend(uint8_t process, const uint8_t *buffer, uint16_t length)
{
    stFrmProcContext *context;
    eDrvStatus lDrvStatus;

    context = frmProcGetContext(process);
    if ((context == NULL) || (buffer == NULL) || (length == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (!context->isInitialized) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    frmProcRefreshState(context);
    if (!context->info.isConfigured) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    lDrvStatus = drvUsbTransmit(DRVUSB_DEV0, DRVUSB_PORT_CDC_DATA_IN_EP, buffer, length);
    if (lDrvStatus == DRV_STATUS_OK) {
        context->info.txBytes += length;
    }

    return frmProcMapDrvStatus(lDrvStatus);
}

eFrmProcStatus frmProcRead(uint8_t process, uint8_t *buffer, uint16_t length, uint16_t *actualLength)
{
    stFrmProcContext *context;
    uint32_t lReadLength;
    uint32_t lUsedLength;

    if (actualLength != NULL) {
        *actualLength = 0U;
    }

    context = frmProcGetContext(process);
    if ((context == NULL) || (buffer == NULL) || (length == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (!context->isInitialized) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    lUsedLength = ringBufferGetUsed(&context->rxRingBuffer);
    if (lUsedLength == 0U) {
        return FRM_PROC_STATUS_EMPTY;
    }

    lReadLength = lUsedLength;
    if (lReadLength > length) {
        lReadLength = length;
    }

    lReadLength = ringBufferRead(&context->rxRingBuffer, buffer, lReadLength);
    if (actualLength != NULL) {
        *actualLength = (uint16_t)lReadLength;
    }
    return FRM_PROC_STATUS_OK;
}

uint16_t frmProcGetRxLength(uint8_t process)
{
    stFrmProcContext *context;

    context = frmProcGetContext(process);
    if ((context == NULL) || !context->isInitialized) {
        return 0U;
    }

    return (uint16_t)ringBufferGetUsed(&context->rxRingBuffer);
}

const stFrmProcInfo *frmProcGetStatus(uint8_t process)
{
    stFrmProcContext *context;

    context = frmProcGetContext(process);
    if (context == NULL) {
        return NULL;
    }

    frmProcRefreshState(context);
    return &context->info;
}

/**************************End of file********************************/
