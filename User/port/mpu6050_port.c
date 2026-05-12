/***********************************************************************************
* @file     : mpu6050_port.c
* @brief    : Project-side MPU6050 binding implementation.
* @details  : Keeps MPU6050 unbound in the current product while exposing an
*             explicit ops table so the reusable core no longer relies on weak
*             platform hooks.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "mpu6050_port.h"

#include <string.h>

#include "rtos.h"

static void mpu6050PortLoadDefaultCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->address = MPU6050_IIC_ADDRESS_LOW;
    cfg->accelRange = MPU6050_ACCEL_RANGE_2G;
    cfg->gyroRange = MPU6050_GYRO_RANGE_250DPS;
}

static const stMpu6050IicInterface *mpu6050PortGetIicInterface(eMPU6050MapType device)
{
    (void)device;
    return NULL;
}

static bool mpu6050PortIsValidAssemble(eMPU6050MapType device)
{
    (void)device;
    return false;
}

static uint8_t mpu6050PortGetLinkId(eMPU6050MapType device)
{
    (void)device;
    return 0U;
}

static uint32_t mpu6050PortGetResetDelayMs(void)
{
    return 0U;
}

static uint32_t mpu6050PortGetWakeDelayMs(void)
{
    return 0U;
}

static void mpu6050PortDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

static const stMpu6050Ops gMpu6050PortOps = {
    .loadDefaultCfg = mpu6050PortLoadDefaultCfg,
    .getIicInterface = mpu6050PortGetIicInterface,
    .isValidAssemble = mpu6050PortIsValidAssemble,
    .getLinkId = mpu6050PortGetLinkId,
    .getResetDelayMs = mpu6050PortGetResetDelayMs,
    .getWakeDelayMs = mpu6050PortGetWakeDelayMs,
    .delayMs = mpu6050PortDelayMs,
};

const stMpu6050Ops *mpu6050PortGetOps(void)
{
    return &gMpu6050PortOps;
}

/**************************End of file********************************/