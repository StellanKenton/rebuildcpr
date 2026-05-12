/************************************************************************************
* @file     : gd25qxxx_port.c
* @brief    : GD25Qxxx project port-layer implementation.
* @details  : This file binds each logical GD25Qxxx device to the project drvspi
*             layer and provides an RTOS-aware millisecond delay hook.
***********************************************************************************/
#include "gd25qxxx_port.h"

#include "drvspi_port.h"
#include "rtos.h"

void gd25qxxxLoadPlatformDefaultCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
const stGd25qxxxSpiInterface *gd25qxxxGetPlatformSpiInterface(const stGd25qxxxCfg *cfg);
bool gd25qxxxPlatformIsValidCfg(const stGd25qxxxCfg *cfg);
void gd25qxxxPlatformDelayMs(uint32_t delayMs);

static eDrvStatus gd25qxxxPortSpiInit(uint8_t bus)
{
    return drvSpiInit(bus);
}

static eDrvStatus gd25qxxxPortSpiTransfer(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData)
{
    stDrvSpiTransfer lTransfer;

    lTransfer.writeBuffer = writeBuffer;
    lTransfer.writeLength = writeLength;
    lTransfer.secondWriteBuffer = secondWriteBuffer;
    lTransfer.secondWriteLength = secondWriteLength;
    lTransfer.readBuffer = readBuffer;
    lTransfer.readLength = readLength;
    lTransfer.readFillData = readFillData;
    return drvSpiTransfer(bus, &lTransfer);
}

static const stGd25qxxxSpiInterface gGd25qxxxSpiInterface = {
    .init = gd25qxxxPortSpiInit,
    .transfer = gd25qxxxPortSpiTransfer,
};

void gd25qxxxLoadPlatformDefaultCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)GD25QXXX_DEV_MAX)) {
        return;
    }

    cfg->linkId = DRVSPI_BUS0;
}

const stGd25qxxxSpiInterface *gd25qxxxGetPlatformSpiInterface(const stGd25qxxxCfg *cfg)
{
    if (!gd25qxxxPlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gGd25qxxxSpiInterface;
}

bool gd25qxxxPlatformIsValidCfg(const stGd25qxxxCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId < DRVSPI_MAX);
}

void gd25qxxxPlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

static const stGd25qxxxOps gGd25qxxxPortOps = {
    .loadDefaultCfg = gd25qxxxLoadPlatformDefaultCfg,
    .getSpiInterface = gd25qxxxGetPlatformSpiInterface,
    .isValidCfg = gd25qxxxPlatformIsValidCfg,
    .delayMs = gd25qxxxPlatformDelayMs,
};

const stGd25qxxxOps *gd25qxxxPortGetOps(void)
{
    return &gGd25qxxxPortOps;
}
/**************************End of file********************************/
