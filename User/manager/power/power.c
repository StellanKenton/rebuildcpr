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

#include "main.h"
#include "drvadc.h"
#include "drvgpio.h"
#include "system.h"
#include "../port/drvadc_port.h"
#include "../port/drvgpio_port.h"
#include "../port/pca9535_port.h"

static uint16_t powerCalcVoltage(eDrvAdcPortMap channel, uint16_t rawValue);
static uint8_t powerCalcBatteryLevel(uint16_t battery10Mv);
static bool powerLedShouldDisplayInMode(eSystemMode mode, bool isCharging);
static bool powerLedIsBlinkOn(void);
static bool powerHasChargeDcInput(uint16_t dc10Mv);
static bool powerIsChargingStatusHigh(void);
static bool powerIsChargeDoneStatusHigh(void);
static ePowerChargeState powerResolveChargeState(uint16_t dc10Mv, bool isChargingStatusHigh, bool isChargeDoneStatusHigh);
static void powerChargeStatusUpdate(void);

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

static uint8_t powerCalcBatteryLevel(uint16_t battery10Mv)
{
    if (battery10Mv >= POWER_VOLTAGE_TO_10MV(4100U)) {
        return 5U;
    }

    if (battery10Mv >= POWER_VOLTAGE_TO_10MV(3950U)) {
        return 4U;
    }

    if (battery10Mv >= POWER_VOLTAGE_TO_10MV(3820U)) {
        return 3U;
    }

    if (battery10Mv >= POWER_VOLTAGE_TO_10MV(3700U)) {
        return 2U;
    }

    if (battery10Mv >= POWER_VOLTAGE_TO_10MV(3550U)) {
        return 1U;
    }

    return 0U;
}

static bool powerLedShouldDisplayInMode(eSystemMode mode, bool isCharging)
{
    if ((mode == eSYSTEM_INIT_MODE) || (mode == eSYSTEM_POWERUP_SELFCHECK_MODE)) {
        return isCharging;
    }

    return true;
}

static bool powerLedIsBlinkOn(void)
{
    return ((HAL_GetTick() / POWER_LED_BLINK_HALF_PERIOD_MS) & 0x01U) == 0U;
}

static bool powerHasChargeDcInput(uint16_t dc10Mv)
{
    return dc10Mv > POWER_CHARGE_THRESHOLD_10MV;
}

static bool powerIsChargingStatusHigh(void)
{
    return drvGpioRead(DRVGPIO_BAT_CHARGING_STATUS) == DRVGPIO_PIN_RESET;
}

static bool powerIsChargeDoneStatusHigh(void)
{
    return drvGpioRead(DRVGPIO_BAT_CHARGE_DONE_STATUS) == DRVGPIO_PIN_RESET;
}

static ePowerChargeState powerResolveChargeState(uint16_t dc10Mv, bool isChargingStatusHigh, bool isChargeDoneStatusHigh)
{
    if (powerHasChargeDcInput(dc10Mv) && isChargingStatusHigh) {
        if (isChargeDoneStatusHigh) {
            return ePOWER_CHARGE_STATE_FULL;
        }

        return ePOWER_CHARGE_STATE_CHARGING;
    }

    return ePOWER_CHARGE_STATE_IDLE;
}

static void powerChargeStatusUpdate(void)
{
    gPowerManager.isChargingStatusHigh = powerIsChargingStatusHigh();
    gPowerManager.isChargeDoneStatusHigh = powerIsChargeDoneStatusHigh();
    gPowerManager.chargeState = powerResolveChargeState(gPowerManager.voltage.dcMv,
                                                        gPowerManager.isChargingStatusHigh,
                                                        gPowerManager.isChargeDoneStatusHigh);
}

bool powerInit(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        gPowerManager.status.state = ePOWER_STATE_READY;
        gPowerManager.status.isShutDownRequested = false;
        gPowerManager.chargeState = ePOWER_CHARGE_STATE_IDLE;
        gPowerManager.isChargingStatusHigh = false;
        gPowerManager.isChargeDoneStatusHigh = false;
        gPowerManager.BatLevel = 0U;
        gPowerManager.batLevelBootSyncCount = POWER_BATTERY_BOOT_SYNC_COUNT;
        gPowerManager.batLevelBootSyncPeak = 0U;
        gPowerManager.batLevelBootSyncActive = true;
    }

    return true;
}

bool powerIsReady(void)
{
    return (gPowerManager.status.state == ePOWER_STATE_READY) ||
           (gPowerManager.status.state == ePOWER_STATE_NORMAL) ||
           (gPowerManager.status.state == ePOWER_STATE_STOPPED);
}

void batholdon(void)
{
    drvGpioWrite(DRVGPIO_POWER_ON_CTRL, DRVGPIO_PIN_SET);
}

void batrelease(void)
{
    drvGpioWrite(DRVGPIO_POWER_ON_CTRL, DRVGPIO_PIN_RESET);
}

uint16_t powerGetVoltage(eDrvAdcPortMap channel)
{
    uint16_t lAdcValue = 0U;

    if (drvAdcReadRaw((uint8_t)channel, &lAdcValue) != DRV_STATUS_OK) {
        return 0U;
    }

    return powerCalcVoltage(channel, lAdcValue);
}

void powerBatteryUpdate(void)
{
    uint8_t lNewLevel = powerCalcBatteryLevel(gPowerManager.voltage.batteryMv);
    bool lHasDcInput = powerHasChargeDcInput(gPowerManager.voltage.dcMv);

    if (gPowerManager.batLevelBootSyncActive && !lHasDcInput) {
        if (lNewLevel > gPowerManager.batLevelBootSyncPeak) {
            gPowerManager.batLevelBootSyncPeak = lNewLevel;
        }

        if (gPowerManager.batLevelBootSyncPeak > gPowerManager.BatLevel) {
            gPowerManager.BatLevel = gPowerManager.batLevelBootSyncPeak;
        }

        if (gPowerManager.batLevelBootSyncCount > 0U) {
            gPowerManager.batLevelBootSyncCount--;
        }

        if (gPowerManager.batLevelBootSyncCount == 0U) {
            gPowerManager.batLevelBootSyncActive = false;
        }
        return;
    }

    gPowerManager.batLevelBootSyncActive = false;

    if (lNewLevel < gPowerManager.BatLevel) {
        gPowerManager.BatLevel = lNewLevel;
        return;
    }

    if (lNewLevel > gPowerManager.BatLevel && lHasDcInput) {
        gPowerManager.BatLevel = lNewLevel;
    }
}

uint8_t powerBatteryGet(void)
{
    return gPowerManager.BatLevel;
}

void powerLedProcess(void)
{
    eSystemMode lMode = systemGetMode();
    uint8_t lBatteryLevel = powerBatteryGet();
    bool lHasDcInput = powerHasChargeDcInput(gPowerManager.voltage.dcMv);
    bool lIsChargeDone = (gPowerManager.chargeState == ePOWER_CHARGE_STATE_FULL);
    bool lIsCharging = (gPowerManager.chargeState == ePOWER_CHARGE_STATE_CHARGING);
    bool lIsRedOn = false;
    bool lIsGreenOn = false;

    if (!powerLedShouldDisplayInMode(lMode, lIsCharging)) {
        (void)pca9535PortLedPowerShow(false, false, false);
        return;
    }

    if (lIsCharging) {
        lIsGreenOn = powerLedIsBlinkOn();
    } else if (lHasDcInput && lIsChargeDone) {
        lIsGreenOn = true;
    } else if (lBatteryLevel <= POWER_BATTERY_LOW_LEVEL_MAX) {
        lIsRedOn = true;
    } else {
        lIsGreenOn = true;
    }

    (void)pca9535PortLedPowerShow(lIsRedOn, lIsGreenOn, false);
}

void powerTransRawToVoltage(void)
{
    uint8_t lIndex;
    const stDrvAdcOps *lOps;
    stDrvAdcData *lAdcData;

    lOps = drvAdcPortGetOps();
    lAdcData = ((lOps != NULL) && (lOps->getData != NULL)) ? lOps->getData() : NULL;
    if (lAdcData == NULL) {
        for (lIndex = 0U; lIndex < (uint8_t)(sizeof(gPowerChannelMap) / sizeof(gPowerChannelMap[0])); lIndex++) {
            *gPowerChannelMap[lIndex].rawValue = 0U;
            *gPowerChannelMap[lIndex].voltageValue = 0U;
        }
        return;
    }

    for (lIndex = 0U; lIndex < (uint8_t)(sizeof(gPowerChannelMap) / sizeof(gPowerChannelMap[0])); lIndex++) {
        uint16_t lRawValue = lAdcData[gPowerChannelMap[lIndex].channel].raw;

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
    powerChargeStatusUpdate();
    powerBatteryUpdate();
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
