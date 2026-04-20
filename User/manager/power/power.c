/***********************************************************************************
* @file     : power.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "power.h"

#include "drvadc.h"

typedef struct stPowerChannelMap {
    eDrvAdcPortMap channel;
    uint16_t *rawValue;
    uint16_t *voltageValue;
} stPowerChannelMap;

static uint16_t powerCalcVoltage(eDrvAdcPortMap channel, uint16_t rawValue);

static PowerManager gPowerManager = {
    .status = {
        .state = ePOWER_STATE_UNINIT,
        .isShutDownRequested = false,
    },
};

static const stPowerChannelMap gPowerChannelMap[] = {
    {
        .channel = DRVADC_BAT,
        .rawValue = &gPowerManager.raw.battery,
        .voltageValue = &gPowerManager.voltage.batteryMv,
    },
    {
        .channel = DRVADC_DC,
        .rawValue = &gPowerManager.raw.dc,
        .voltageValue = &gPowerManager.voltage.dcMv,
    },
    {
        .channel = DRVADC_5V0,
        .rawValue = &gPowerManager.raw.v5v0,
        .voltageValue = &gPowerManager.voltage.v5v0Mv,
    },
    {
        .channel = DRVADC_3V3,
        .rawValue = &gPowerManager.raw.v3v3,
        .voltageValue = &gPowerManager.voltage.v3v3Mv,
    },
};

static uint16_t powerCalcVoltage(eDrvAdcPortMap channel, uint16_t rawValue)
{
    uint32_t lVoltage = 0U;

    switch (channel) {
        case DRVADC_BAT:
            lVoltage = ((uint32_t)rawValue * 205260U) / 253890U;
            break;

        case DRVADC_DC:
            lVoltage = ((uint32_t)rawValue * 205260U) / 253890U;
            break;

        case DRVADC_5V0:
            lVoltage = ((uint32_t)rawValue * 14850U) / 61425U;
            break;

        case DRVADC_3V3:
            lVoltage = ((uint32_t)rawValue * 13200U) / 40950U;
            break;

        default:
            break;
    }

    return (uint16_t)lVoltage;
}

bool powerInit(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        gPowerManager.status.state = ePOWER_STATE_READY;
        gPowerManager.status.isShutDownRequested = false;
    }

    return true;
}

uint16_t powerGetVoltage(eDrvAdcPortMap channel)
{
    uint16_t lAdcValue = 0U;

    if (drvAdcReadRaw((uint8_t)channel, &lAdcValue) != DRV_STATUS_OK) {
        return 0U;
    }

    return powerCalcVoltage(channel, lAdcValue);
}

void powerTransRawToVoltage(void)
{
    uint8_t lIndex;
    uint16_t lRawValue;

    for (lIndex = 0U; lIndex < (uint8_t)(sizeof(gPowerChannelMap) / sizeof(gPowerChannelMap[0])); lIndex++) {
        if (drvAdcReadRaw((uint8_t)gPowerChannelMap[lIndex].channel, &lRawValue) != DRV_STATUS_OK) {
            *gPowerChannelMap[lIndex].rawValue = 0U;
            *gPowerChannelMap[lIndex].voltageValue = 0U;
            continue;
        }

        *gPowerChannelMap[lIndex].rawValue = lRawValue;
        *gPowerChannelMap[lIndex].voltageValue = powerCalcVoltage(gPowerChannelMap[lIndex].channel, lRawValue);
    }
}

void powerProcess(void)
{
    if ((gPowerManager.status.state != ePOWER_STATE_READY) &&
        (gPowerManager.status.state != ePOWER_STATE_NORMAL) &&
        (gPowerManager.status.state != ePOWER_STATE_STOPPED)) {
        return;
    }

    powerTransRawToVoltage();
    gPowerManager.status.state = gPowerManager.status.isShutDownRequested ? ePOWER_STATE_STOPPED : ePOWER_STATE_NORMAL;
}

bool powerRequestShutDown(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        return false;
    }

    gPowerManager.status.isShutDownRequested = true;
    return true;
}

bool powerRequestPowerUp(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        return false;
    }

    gPowerManager.status.isShutDownRequested = false;
    return true;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerManager.status;
}

const PowerManager *powerGetManager(void)
{
    return &gPowerManager;
}

/**************************End of file********************************/
