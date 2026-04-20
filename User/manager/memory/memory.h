/************************************************************************************
* @file     : memory.h
* @brief    : Project-side flash memory manager.
* @details  : Adapts the legacy app_memory area layout to the current project
*             manager style and GD25Qxxx flash module.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MEMORY_H
#define REBUILDCPR_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_LOG_TAG                       "memory"
#define MEMORY_PROGRAM_SECTOR_COUNT          1024UL
#define MEMORY_PROGRAM_SECTOR_SIZE           4096UL
#define MEMORY_CPR_ACTIVE_SECTOR_GROUP       4U
#define MEMORY_DATA_BLOCK_SIZE               16U

typedef enum eMemoryState {
    eMEMORY_STATE_UNINIT = 0,
    eMEMORY_STATE_READY,
    eMEMORY_STATE_ACTIVE,
    eMEMORY_STATE_ERROR,
} eMemoryState;

typedef enum eMemoryArea {
    eMEMORY_AREA_FACTORY_INFO = 0,
    eMEMORY_AREA_VOLUME = 3,
    eMEMORY_AREA_METRONOME = 4,
    eMEMORY_AREA_WIFI = 6,
    eMEMORY_AREA_SECRET = 7,
    eMEMORY_AREA_UTC = 8,
    eMEMORY_AREA_CPR_START = 128,
    eMEMORY_AREA_CPR_END = 176,
    eMEMORY_AREA_LOG_START = 528,
    eMEMORY_AREA_LOG_END = 576,
    eMEMORY_AREA_BOOT_FLAG = 760,
    eMEMORY_AREA_APP1_START = 768,
    eMEMORY_AREA_APP2_START = 896,
} eMemoryArea;

typedef struct stMemoryStatus {
    eMemoryState state;
    bool flashReady;
} stMemoryStatus;

typedef struct stMemoryStorageInfo {
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
    uint8_t addressWidth;
    uint16_t pageSizeBytes;
    uint32_t totalSizeBytes;
    uint32_t sectorSizeBytes;
    uint32_t blockSizeBytes;
} stMemoryStorageInfo;

typedef struct stMemoryManager {
    stMemoryStatus status;
    stMemoryStorageInfo storage;
} stMemoryManager;

typedef struct stMemorySingleStorage {
    eMemoryArea area;
    uint16_t dataSize;
    void *dataPtr;
} stMemorySingleStorage;

bool memoryInit(void);
void memoryProcess(void);
bool memoryGetAreaRange(eMemoryArea area, uint32_t *startAddress, uint32_t *lengthBytes);
bool memorySingleRead(stMemorySingleStorage *storage);
bool memorySingleWrite(const stMemorySingleStorage *storage);
bool memoryEraseArea(eMemoryArea area);
const stMemoryStatus *memoryGetStatus(void);
const stMemoryManager *memoryGetManager(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
