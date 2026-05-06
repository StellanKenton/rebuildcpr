/***********************************************************************************
* @file     : wt2003hx_port.c
* @brief    : Project-side WT2003HX binding implementation.
* @details  : Provides UART transport and enable GPIO hooks for the current board.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wt2003hx_port.h"

#include <string.h>

#include "main.h"

#include "drvgpio.h"
#include "drvgpio_port.h"
#include "drvuart.h"
#include "drvuart_port.h"
#include "rtos.h"

static eDrvStatus wt2003hxPortTransportInit(uint8_t linkId);
static eDrvStatus wt2003hxPortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
static uint16_t wt2003hxPortTransportGetRxLen(uint8_t linkId);
static eDrvStatus wt2003hxPortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length);
static uint32_t wt2003hxPortTransportGetTickMs(void);
static void wt2003hxPortControlSetEnable(uint8_t enablePin, bool enabled);
static void wt2003hxPortControlDelayMs(uint32_t delayMs);

static const stWt2003hxPortTransportInterface gWt2003hxPortTransportInterface = {
    .init = wt2003hxPortTransportInit,
    .write = wt2003hxPortTransportWrite,
    .getRxLen = wt2003hxPortTransportGetRxLen,
    .read = wt2003hxPortTransportRead,
    .getTickMs = wt2003hxPortTransportGetTickMs,
};

static const stWt2003hxPortControlInterface gWt2003hxPortControlInterface = {
    .setEnable = wt2003hxPortControlSetEnable,
    .delayMs = wt2003hxPortControlDelayMs,
};

static eDrvStatus wt2003hxPortTransportInit(uint8_t linkId)
{
    return drvUartInit(linkId);
}

static eDrvStatus wt2003hxPortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    return drvUartTransmit(linkId, buffer, length, timeoutMs);
}

static uint16_t wt2003hxPortTransportGetRxLen(uint8_t linkId)
{
    return drvUartGetDataLen(linkId);
}

static eDrvStatus wt2003hxPortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length)
{
    return drvUartReceive(linkId, buffer, length);
}

static uint32_t wt2003hxPortTransportGetTickMs(void)
{
    if (repRtosIsSchedulerRunning()) {
        return repRtosGetTickMs();
    }

    return HAL_GetTick();
}

static void wt2003hxPortControlSetEnable(uint8_t enablePin, bool enabled)
{
    drvGpioInit();
    drvGpioWrite(enablePin, enabled ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

static void wt2003hxPortControlDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

void wt2003hxLoadPlatformDefaultCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->linkId = DRVUART_AUDIO;
    cfg->enablePin = DRVGPIO_EN_AUDIO;
    cfg->powerDelayMs = WT2003HX_POWER_DELAY_MS;
    cfg->txTimeoutMs = WT2003HX_TX_TIMEOUT_MS;
}

const stWt2003hxTransportInterface *wt2003hxGetPlatformTransportInterface(const stWt2003hxCfg *cfg)
{
    return wt2003hxPlatformIsValidCfg(cfg) ? &gWt2003hxPortTransportInterface : NULL;
}

const stWt2003hxControlInterface *wt2003hxGetPlatformControlInterface(eWt2003hxMapType device)
{
    return (device == WT2003HX_DEV0) ? &gWt2003hxPortControlInterface : NULL;
}

bool wt2003hxPlatformIsValidCfg(const stWt2003hxCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId < DRVUART_MAX) && (cfg->enablePin < DRVGPIO_MAX);
}

eDrvStatus wt2003hxPortInit(void)
{
    return wt2003hxInit(WT2003HX_DEV0);
}

eDrvStatus wt2003hxPortProcess(void)
{
    return wt2003hxProcess(WT2003HX_DEV0);
}

bool wt2003hxPortIsReady(void)
{
    return wt2003hxIsReady(WT2003HX_DEV0);
}

eDrvStatus wt2003hxPortPlayName(const uint8_t *name, uint8_t nameLen)
{
    return wt2003hxPlayName(WT2003HX_DEV0, name, nameLen);
}

eDrvStatus wt2003hxPortStop(void)
{
    return wt2003hxStop(WT2003HX_DEV0);
}

eDrvStatus wt2003hxPortSetVolume(uint8_t volume)
{
    return wt2003hxSetVolume(WT2003HX_DEV0, volume);
}

eDrvStatus wt2003hxPortSetPlayMode(eWt2003hxPlayMode mode)
{
    return wt2003hxSetPlayMode(WT2003HX_DEV0, mode);
}

eDrvStatus wt2003hxPortSetOutputMode(eWt2003hxOutputMode mode)
{
    return wt2003hxSetOutputMode(WT2003HX_DEV0, mode);
}

eDrvStatus wt2003hxPortQuery(uint8_t cmd)
{
    return wt2003hxQuery(WT2003HX_DEV0, cmd);
}

bool wt2003hxPortGetInfo(stWt2003hxInfo *info)
{
    return wt2003hxGetInfo(WT2003HX_DEV0, info);
}

/**************************End of file********************************/
