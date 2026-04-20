/***********************************************************************************
* @file     : memory.c
* @brief    : Project-side flash memory manager.
* @details  : Keeps the legacy memory area partitioning while using the current
*             GD25Qxxx reusable module and project manager conventions.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "memory.h"

#include <string.h>

#include "../../../rep/module/gd25qxxx/gd25qxxx.h"
#include "../../../rep/service/log/log.h"

#define MEMORY_DEVICE                        GD25Q32_MEM
#define MEMORY_APP_SLOT_SECTOR_COUNT         128UL

typedef struct stMemoryAreaRange {
    uint32_t startSector;
    uint32_t endSector;
} stMemoryAreaRange;

static bool memoryLoadFlashInfo(void);
static bool memoryTrySetReady(void);
static bool memoryGetAreaSectors(eMemoryArea area, stMemoryAreaRange *range);
static bool memoryIsRangeInsideFlash(uint32_t startAddress, uint32_t lengthBytes);
static bool memoryDoRead(uint32_t address, void *buffer, uint32_t lengthBytes);
static bool memoryDoWrite(uint32_t address, const void *buffer, uint32_t lengthBytes);
static bool memoryDoErase(uint32_t startAddress, uint32_t lengthBytes);

static stMemoryManager gMemoryManager = {
    .status = {
        .state = eMEMORY_STATE_UNINIT,
        .flashReady = false,
    },
};

static bool memoryLoadFlashInfo(void)
{
    const stGd25qxxxInfo *lInfo = gd25qxxxGetInfo(MEMORY_DEVICE);

    if (lInfo == NULL) {
        return false;
    }

    gMemoryManager.storage.manufacturerId = lInfo->manufacturerId;
    gMemoryManager.storage.memoryType = lInfo->memoryType;
    gMemoryManager.storage.capacityId = lInfo->capacityId;
    gMemoryManager.storage.addressWidth = lInfo->addressWidth;
    gMemoryManager.storage.pageSizeBytes = lInfo->pageSizeBytes;
    gMemoryManager.storage.totalSizeBytes = lInfo->totalSizeBytes;
    gMemoryManager.storage.sectorSizeBytes = lInfo->sectorSizeBytes;
    gMemoryManager.storage.blockSizeBytes = lInfo->blockSizeBytes;
    return true;
}

static bool memoryTrySetReady(void)
{
    eGd25qxxxStatus lStatus = gd25qxxxInit(MEMORY_DEVICE);

    if (lStatus != GD25QXXX_STATUS_OK) {
        gMemoryManager.status.state = eMEMORY_STATE_ERROR;
        gMemoryManager.status.flashReady = false;
        LOG_E(MEMORY_LOG_TAG, "flash init fail status=%d", (int)lStatus);
        return false;
    }

    if (!memoryLoadFlashInfo()) {
        gMemoryManager.status.state = eMEMORY_STATE_ERROR;
        gMemoryManager.status.flashReady = false;
        LOG_E(MEMORY_LOG_TAG, "flash info unavailable after init");
        return false;
    }

    gMemoryManager.status.state = eMEMORY_STATE_READY;
    gMemoryManager.status.flashReady = true;
    LOG_I(MEMORY_LOG_TAG,
          "flash ready jedec=%02X %02X %02X size=%lu",
          (unsigned int)gMemoryManager.storage.manufacturerId,
          (unsigned int)gMemoryManager.storage.memoryType,
          (unsigned int)gMemoryManager.storage.capacityId,
          (unsigned long)gMemoryManager.storage.totalSizeBytes);
    return true;
}

static bool memoryGetAreaSectors(eMemoryArea area, stMemoryAreaRange *range)
{
    if (range == NULL) {
        return false;
    }

    switch (area) {
        case eMEMORY_AREA_FACTORY_INFO:
        case eMEMORY_AREA_VOLUME:
        case eMEMORY_AREA_METRONOME:
        case eMEMORY_AREA_WIFI:
        case eMEMORY_AREA_SECRET:
        case eMEMORY_AREA_UTC:
        case eMEMORY_AREA_BOOT_FLAG:
            range->startSector = (uint32_t)area;
            range->endSector = (uint32_t)area;
            return true;

        case eMEMORY_AREA_CPR_START:
            range->startSector = (uint32_t)eMEMORY_AREA_CPR_START;
            range->endSector = (uint32_t)eMEMORY_AREA_CPR_END;
            return true;

        case eMEMORY_AREA_LOG_START:
            range->startSector = (uint32_t)eMEMORY_AREA_LOG_START;
            range->endSector = (uint32_t)eMEMORY_AREA_LOG_END;
            return true;

        case eMEMORY_AREA_APP1_START:
            range->startSector = (uint32_t)eMEMORY_AREA_APP1_START;
            range->endSector = (uint32_t)eMEMORY_AREA_APP1_START + MEMORY_APP_SLOT_SECTOR_COUNT - 1UL;
            return true;

        case eMEMORY_AREA_APP2_START:
            range->startSector = (uint32_t)eMEMORY_AREA_APP2_START;
            range->endSector = (uint32_t)eMEMORY_AREA_APP2_START + MEMORY_APP_SLOT_SECTOR_COUNT - 1UL;
            return true;

        default:
            return false;
    }
}

static bool memoryIsRangeInsideFlash(uint32_t startAddress, uint32_t lengthBytes)
{
    uint32_t lEndAddress;

    if (!gMemoryManager.status.flashReady) {
        return false;
    }

    if (lengthBytes == 0U) {
        return startAddress <= gMemoryManager.storage.totalSizeBytes;
    }

    lEndAddress = startAddress + lengthBytes;
    if (lEndAddress < startAddress) {
        return false;
    }

    return lEndAddress <= gMemoryManager.storage.totalSizeBytes;
}

static bool memoryDoRead(uint32_t address, void *buffer, uint32_t lengthBytes)
{
    eGd25qxxxStatus lStatus;

    if ((buffer == NULL) && (lengthBytes > 0U)) {
        return false;
    }

    if (!memoryIsRangeInsideFlash(address, lengthBytes)) {
        return false;
    }

    if (lengthBytes == 0U) {
        return true;
    }

    gMemoryManager.status.state = eMEMORY_STATE_ACTIVE;
    lStatus = gd25qxxxRead(MEMORY_DEVICE, address, (uint8_t *)buffer, lengthBytes);
    gMemoryManager.status.state = (lStatus == GD25QXXX_STATUS_OK) ? eMEMORY_STATE_READY : eMEMORY_STATE_ERROR;
    gMemoryManager.status.flashReady = (lStatus == GD25QXXX_STATUS_OK);
    return lStatus == GD25QXXX_STATUS_OK;
}

static bool memoryDoWrite(uint32_t address, const void *buffer, uint32_t lengthBytes)
{
    eGd25qxxxStatus lStatus;

    if ((buffer == NULL) && (lengthBytes > 0U)) {
        return false;
    }

    if (!memoryIsRangeInsideFlash(address, lengthBytes)) {
        return false;
    }

    if (lengthBytes == 0U) {
        return true;
    }

    gMemoryManager.status.state = eMEMORY_STATE_ACTIVE;
    lStatus = gd25qxxxWrite(MEMORY_DEVICE, address, (const uint8_t *)buffer, lengthBytes);
    gMemoryManager.status.state = (lStatus == GD25QXXX_STATUS_OK) ? eMEMORY_STATE_READY : eMEMORY_STATE_ERROR;
    gMemoryManager.status.flashReady = (lStatus == GD25QXXX_STATUS_OK);
    return lStatus == GD25QXXX_STATUS_OK;
}

static bool memoryDoErase(uint32_t startAddress, uint32_t lengthBytes)
{
    uint32_t lAddress;

    if ((lengthBytes == 0U) ||
        ((startAddress % MEMORY_PROGRAM_SECTOR_SIZE) != 0U) ||
        ((lengthBytes % MEMORY_PROGRAM_SECTOR_SIZE) != 0U) ||
        !memoryIsRangeInsideFlash(startAddress, lengthBytes)) {
        return false;
    }

    gMemoryManager.status.state = eMEMORY_STATE_ACTIVE;
    for (lAddress = startAddress; lAddress < (startAddress + lengthBytes); lAddress += MEMORY_PROGRAM_SECTOR_SIZE) {
        if (gd25qxxxEraseSector(MEMORY_DEVICE, lAddress) != GD25QXXX_STATUS_OK) {
            gMemoryManager.status.state = eMEMORY_STATE_ERROR;
            gMemoryManager.status.flashReady = false;
            return false;
        }
    }

    gMemoryManager.status.state = eMEMORY_STATE_READY;
    gMemoryManager.status.flashReady = true;
    return true;
}

bool memoryInit(void)
{
    if (gMemoryManager.status.flashReady) {
        return true;
    }

    return memoryTrySetReady();
}

void memoryProcess(void)
{
    bool lIsReady;

    if (gMemoryManager.status.state == eMEMORY_STATE_UNINIT) {
        return;
    }

    lIsReady = gd25qxxxIsReady(MEMORY_DEVICE);
    gMemoryManager.status.flashReady = lIsReady;
    if (!lIsReady) {
        gMemoryManager.status.state = eMEMORY_STATE_ERROR;
        return;
    }

    if (gMemoryManager.status.state != eMEMORY_STATE_ACTIVE) {
        gMemoryManager.status.state = eMEMORY_STATE_READY;
    }
}

bool memoryGetAreaRange(eMemoryArea area, uint32_t *startAddress, uint32_t *lengthBytes)
{
    stMemoryAreaRange lRange;
    uint32_t lSectorCount;

    if ((startAddress == NULL) || (lengthBytes == NULL) || !memoryGetAreaSectors(area, &lRange)) {
        return false;
    }

    lSectorCount = lRange.endSector - lRange.startSector + 1UL;
    *startAddress = lRange.startSector * MEMORY_PROGRAM_SECTOR_SIZE;
    *lengthBytes = lSectorCount * MEMORY_PROGRAM_SECTOR_SIZE;
    return true;
}

bool memorySingleRead(stMemorySingleStorage *storage)
{
    uint32_t lStartAddress;
    uint32_t lLengthBytes;

    if ((storage == NULL) || (storage->dataPtr == NULL)) {
        return false;
    }

    if (!memoryGetAreaRange(storage->area, &lStartAddress, &lLengthBytes) ||
        ((uint32_t)storage->dataSize > lLengthBytes)) {
        return false;
    }

    return memoryDoRead(lStartAddress, storage->dataPtr, storage->dataSize);
}

bool memorySingleWrite(const stMemorySingleStorage *storage)
{
    uint32_t lStartAddress;
    uint32_t lLengthBytes;

    if ((storage == NULL) || (storage->dataPtr == NULL)) {
        return false;
    }

    if (!memoryGetAreaRange(storage->area, &lStartAddress, &lLengthBytes) ||
        ((uint32_t)storage->dataSize > lLengthBytes)) {
        return false;
    }

    if (!memoryDoErase(lStartAddress, lLengthBytes)) {
        LOG_E(MEMORY_LOG_TAG, "erase area fail area=%lu", (unsigned long)storage->area);
        return false;
    }

    if (!memoryDoWrite(lStartAddress, storage->dataPtr, storage->dataSize)) {
        LOG_E(MEMORY_LOG_TAG, "write area fail area=%lu len=%u", (unsigned long)storage->area, (unsigned int)storage->dataSize);
        return false;
    }

    return true;
}

bool memoryEraseArea(eMemoryArea area)
{
    uint32_t lStartAddress;
    uint32_t lLengthBytes;

    if (!memoryGetAreaRange(area, &lStartAddress, &lLengthBytes)) {
        return false;
    }

    return memoryDoErase(lStartAddress, lLengthBytes);
}

const stMemoryStatus *memoryGetStatus(void)
{
    return &gMemoryManager.status;
}

const stMemoryManager *memoryGetManager(void)
{
    return &gMemoryManager;
}

/**************************End of file********************************/
