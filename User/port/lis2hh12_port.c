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

static bool lis2hh12PortIsValidAssemble(eLis2hh12MapType device);

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

static void lis2hh12PortLoadDefaultCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
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

static const stLis2hh12IicInterface *lis2hh12PortGetIicInterface(eLis2hh12MapType device)
{
    if (!lis2hh12PortIsValidAssemble(device)) {
        return NULL;
    }

    return &gLis2hh12IicInterface;
}

static bool lis2hh12PortIsValidAssemble(eLis2hh12MapType device)
{
    return ((uint32_t)device < (uint32_t)LIS2HH12_DEV_MAX) &&
           (gLis2hh12LinkMap[device] < DRVIIC_MAX);
}

static uint8_t lis2hh12PortGetLinkId(eLis2hh12MapType device)
{
    if ((uint32_t)device >= (uint32_t)LIS2HH12_DEV_MAX) {
        return 0U;
    }

    return gLis2hh12LinkMap[device];
}

static uint32_t lis2hh12PortGetRetryDelayMs(void)
{
    return LIS2HH12_PORT_RETRY_DELAY_MS;
}

static uint32_t lis2hh12PortGetResetPollDelayMs(void)
{
    return LIS2HH12_PORT_RESET_POLL_DELAY_MS;
}

static void lis2hh12PortDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

static const stLis2hh12Ops gLis2hh12PortOps = {
    .loadDefaultCfg = lis2hh12PortLoadDefaultCfg,
    .getIicInterface = lis2hh12PortGetIicInterface,
    .isValidAssemble = lis2hh12PortIsValidAssemble,
    .getLinkId = lis2hh12PortGetLinkId,
    .getRetryDelayMs = lis2hh12PortGetRetryDelayMs,
    .getResetPollDelayMs = lis2hh12PortGetResetPollDelayMs,
    .delayMs = lis2hh12PortDelayMs,
};

const stLis2hh12Ops *lis2hh12PortGetOps(void)
{
    return &gLis2hh12PortOps;
}

/**************************End of file********************************/
