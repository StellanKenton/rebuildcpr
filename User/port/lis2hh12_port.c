/***********************************************************************************
* @file     : lis2hh12_port.c
* @brief    : Project-side LIS2HH12 port implementation.
* @details  : Uses hardware IIC bus 0, mapped by bspiic to hi2c1 at 100 kHz.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "lis2hh12_port.h"

#include "drviic.h"
#include "drviic_port.h"
#include "rtos.h"

static eDrvStatus lis2hh12PortInit(uint8_t bus)
{
    return drvIicInit(bus);
}

static eDrvStatus lis2hh12PortWriteReg(uint8_t bus,
                                       uint8_t address,
                                       const uint8_t *regBuf,
                                       uint16_t regLen,
                                       const uint8_t *buffer,
                                       uint16_t length)
{
    return drvIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus lis2hh12PortReadReg(uint8_t bus,
                                      uint8_t address,
                                      const uint8_t *regBuf,
                                      uint16_t regLen,
                                      uint8_t *buffer,
                                      uint16_t length)
{
    return drvIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}

static const stLis2hh12IicInterface gLis2hh12IicInterface = {
    .init = lis2hh12PortInit,
    .writeReg = lis2hh12PortWriteReg,
    .readReg = lis2hh12PortReadReg,
};

static const uint8_t gLis2hh12LinkMap[LIS2HH12_DEV_MAX] = {
    [LIS2HH12_DEV0] = DRVIIC_BUS0,
};

void lis2hh12LoadPlatformDefaultCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)LIS2HH12_DEV_MAX)) {
        return;
    }

    cfg->address = LIS2HH12_IIC_ADDRESS_LOW;
    cfg->fifoWatermark = FIFO_THRESHOLD_CONFIG;
    cfg->retryMax = ACC_COMMUNICATE_MAX_RETRY;
    cfg->dataRate = LIS2HH12_DATA_RATE_100HZ;
    cfg->fullScale = LIS2HH12_FULL_SCALE_4G;
    cfg->filterIntPath = LIS2HH12_FILTER_INT_DISABLE;
    cfg->filterOutPath = LIS2HH12_FILTER_OUT_LP;
    cfg->filterLowBandwidth = LIS2HH12_FILTER_LOW_BW_ODR_DIV_9;
    cfg->filterAntiAliasBandwidth = LIS2HH12_FILTER_AA_AUTO;
    cfg->fifoMode = LIS2HH12_FIFO_MODE_STREAM;
    cfg->blockDataUpdate = true;
    cfg->autoIncrement = true;
}

const stLis2hh12IicInterface *lis2hh12GetPlatformIicInterface(eLis2hh12MapType device)
{
    if (!lis2hh12PlatformIsValidAssemble(device)) {
        return NULL;
    }

    return &gLis2hh12IicInterface;
}

bool lis2hh12PlatformIsValidAssemble(eLis2hh12MapType device)
{
    return ((uint32_t)device < (uint32_t)LIS2HH12_DEV_MAX) &&
           (gLis2hh12LinkMap[device] < DRVIIC_MAX);
}

uint8_t lis2hh12PlatformGetLinkId(eLis2hh12MapType device)
{
    if ((uint32_t)device >= (uint32_t)LIS2HH12_DEV_MAX) {
        return 0U;
    }

    return gLis2hh12LinkMap[device];
}

uint32_t lis2hh12PlatformGetRetryDelayMs(void)
{
    return LIS2HH12_PORT_RETRY_DELAY_MS;
}

uint32_t lis2hh12PlatformGetResetPollDelayMs(void)
{
    return LIS2HH12_PORT_RESET_POLL_DELAY_MS;
}

void lis2hh12PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

/**************************End of file********************************/
