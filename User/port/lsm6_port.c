/***********************************************************************************
* @file     : lsm6_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "lsm6_port.h"

#include "main.h"
#include "rtos.h"

typedef enum eLsm6LocalBus {
    LSM6_LOCAL_BUS0 = 0,
} eLsm6LocalBus;

static bool gLsm6PortCycleCntReady = false;

static void lsm6PortEnableCycleCnt(void)
{
    ...(truncated)
    }

    for (index = 0U; index < regLen; ++index) {
        status = lsm6PortWriteByte(regBuf[index]);
        if (status != DRV_STATUS_OK) {
            lsm6PortSendStop();
            return status;
        }
    }

    lsm6PortSendStart();
    status = lsm6PortWriteByte((uint8_t)((address << 1U) | 0x01U));
    if (status != DRV_STATUS_OK) {
        lsm6PortSendStop();
        return status;
    }

    for (index = 0U; index < length; ++index) {
        buffer[index] = lsm6PortReadByte(index < (uint16_t)(length - 1U));
    }

    lsm6PortSendStop();
    return DRV_STATUS_OK;
}

static const stLsm6IicInterface gLsm6IicInterface = {
    .init = lsm6PortInit,
    .writeReg = lsm6PortWriteReg,
    .readReg = lsm6PortReadReg,
};

void lsm6LoadPlatformDefaultCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)LSM6_DEV_MAX)) {
        return;
    }

    cfg->address = LSM6_IIC_ADDRESS_LOW;
    cfg->accelDataRate = LSM6_ACCEL_ODR_104HZ;
    cfg->gyroDataRate = LSM6_GYRO_ODR_104HZ;
    cfg->accelRange = LSM6_ACCEL_RANGE_4G;
    cfg->gyroRange = LSM6_GYRO_RANGE_2000DPS;
    cfg->blockDataUpdate = true;
    cfg->autoIncrement = true;
}

const stLsm6IicInterface *lsm6GetPlatformIicInterface(eLsm6MapType device)
{
    return lsm6PlatformIsValidAssemble(device) ? &gLsm6IicInterface : NULL;
}

bool lsm6PlatformIsValidAssemble(eLsm6MapType device)
{
    return ((uint32_t)device < (uint32_t)LSM6_DEV_MAX);
}

uint8_t lsm6PlatformGetLinkId(eLsm6MapType device)
{
    (void)device;
    return LSM6_LOCAL_BUS0;
}

uint32_t lsm6PlatformGetResetDelayMs(void)
{
    return LSM6_PORT_RESET_DELAY_MS;
}

uint32_t lsm6PlatformGetResetPollDelayMs(void)
{
    return LSM6_PORT_RESET_POLL_DELAY_MS;
}

void lsm6PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

/**************************End of file********************************/