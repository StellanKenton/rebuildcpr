/***********************************************************************************
* @file     : vfs_littlefs_port.c
* @brief    : Project-side GD25Qxxx binding for the vfs littlefs backend.
* @details  : Computes the usable external flash filesystem region and registers
*             it as the `/mem` vfs mount.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "vfs_littlefs_port.h"

#include "../../Core/Inc/iwdg.h"
#include "../../rep/module/gd25qxxx/gd25qxxx.h"
#include "../../rep/sys/log/log.h"
#include "../../rep/sys/vfs/vfs.h"
#include "../../rep/sys/vfs/vfs_littlefs.h"

#define VFS_LITTLEFS_PORT_LOG_TAG            "vfs_port"
#define VFS_LITTLEFS_PORT_FLASH_DEVICE       GD25Q32_MEM
#define VFS_LITTLEFS_PORT_MOUNT_PATH         "/mem"
#define VFS_LITTLEFS_PORT_REGION_OFFSET      0x00000000UL
#define VFS_LITTLEFS_PORT_REGION_MAX_SIZE    0x00300000UL
#define VFS_LITTLEFS_PORT_RESERVED_TAIL_SIZE 0x00100000UL
#define VFS_LITTLEFS_PORT_READ_SIZE          16U
#define VFS_LITTLEFS_PORT_PROG_SIZE          16U
#define VFS_LITTLEFS_PORT_BLOCK_SIZE         GD25QXXX_SECTOR_SIZE
#define VFS_LITTLEFS_PORT_CACHE_SIZE         GD25QXXX_PAGE_SIZE
#define VFS_LITTLEFS_PORT_LOOKAHEAD_SIZE     32U
#define VFS_LITTLEFS_PORT_BLOCK_CYCLES       500

static bool vfsLittlefsPortBlockInit(void *deviceContext);
static bool vfsLittlefsPortBlockRead(void *deviceContext, uint32_t address, void *buffer, uint32_t size);
static bool vfsLittlefsPortBlockProg(void *deviceContext, uint32_t address, const void *buffer, uint32_t size);
static bool vfsLittlefsPortBlockErase(void *deviceContext, uint32_t address, uint32_t size);
static bool vfsLittlefsPortBlockSync(void *deviceContext);
static void vfsLittlefsPortFeedWatchdog(void);
static uint32_t vfsLittlefsPortGetRegionSize(const stGd25qxxxInfo *info);
static bool vfsLittlefsPortFormatAfterCorrupt(stVfsMountCfg *mountCfg, uint32_t regionSize);
static void vfsLittlefsPortLogReady(const char *state, uint32_t regionSize);

static const stVfsLittlefsBlockDeviceOps gVfsLittlefsPortBlockOps = {
    .init = vfsLittlefsPortBlockInit,
    .read = vfsLittlefsPortBlockRead,
    .prog = vfsLittlefsPortBlockProg,
    .erase = vfsLittlefsPortBlockErase,
    .sync = vfsLittlefsPortBlockSync,
};

static stVfsLittlefsContext gVfsLittlefsPortContext;
static bool gVfsLittlefsPortRegistered = false;
static bool gVfsLittlefsPortReady = false;

static bool vfsLittlefsPortBlockInit(void *deviceContext)
{
    eGd25qxxxMapType lDevice = (eGd25qxxxMapType)(uintptr_t)deviceContext;
    return gd25qxxxInit(lDevice) == GD25QXXX_STATUS_OK;
}

static bool vfsLittlefsPortBlockRead(void *deviceContext, uint32_t address, void *buffer, uint32_t size)
{
    eGd25qxxxMapType lDevice = (eGd25qxxxMapType)(uintptr_t)deviceContext;
    return gd25qxxxRead(lDevice, address, (uint8_t *)buffer, size) == GD25QXXX_STATUS_OK;
}

static bool vfsLittlefsPortBlockProg(void *deviceContext, uint32_t address, const void *buffer, uint32_t size)
{
    eGd25qxxxMapType lDevice = (eGd25qxxxMapType)(uintptr_t)deviceContext;
    bool lResult = gd25qxxxWrite(lDevice, address, (const uint8_t *)buffer, size) == GD25QXXX_STATUS_OK;
    vfsLittlefsPortFeedWatchdog();
    return lResult;
}

static void vfsLittlefsPortFeedWatchdog(void)
{
    if (hiwdg.Instance != NULL) {
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}

static bool vfsLittlefsPortBlockErase(void *deviceContext, uint32_t address, uint32_t size)
{
    eGd25qxxxMapType lDevice = (eGd25qxxxMapType)(uintptr_t)deviceContext;
    uint32_t lAddress = address;

    if ((size == 0U) || ((size % GD25QXXX_SECTOR_SIZE) != 0U)) {
        return false;
    }

    while (size > 0U) {
        if (gd25qxxxEraseSector(lDevice, lAddress) != GD25QXXX_STATUS_OK) {
            return false;
        }
        vfsLittlefsPortFeedWatchdog();

        lAddress += GD25QXXX_SECTOR_SIZE;
        size -= GD25QXXX_SECTOR_SIZE;
    }

    return true;
}

static bool vfsLittlefsPortBlockSync(void *deviceContext)
{
    (void)deviceContext;
    vfsLittlefsPortFeedWatchdog();
    return true;
}

static uint32_t vfsLittlefsPortGetRegionSize(const stGd25qxxxInfo *info)
{
    uint32_t lUsableSize;
    uint32_t lRegionSize;

    if ((info == NULL) || (info->totalSizeBytes <= VFS_LITTLEFS_PORT_REGION_OFFSET)) {
        return 0U;
    }

    lUsableSize = info->totalSizeBytes - VFS_LITTLEFS_PORT_REGION_OFFSET;
    if (lUsableSize > VFS_LITTLEFS_PORT_RESERVED_TAIL_SIZE) {
        lUsableSize -= VFS_LITTLEFS_PORT_RESERVED_TAIL_SIZE;
    } else {
        lUsableSize = 0U;
    }

    lRegionSize = lUsableSize;
    if (lRegionSize > VFS_LITTLEFS_PORT_REGION_MAX_SIZE) {
        lRegionSize = VFS_LITTLEFS_PORT_REGION_MAX_SIZE;
    }

    lRegionSize -= (lRegionSize % VFS_LITTLEFS_PORT_BLOCK_SIZE);
    return lRegionSize;
}

static void vfsLittlefsPortLogReady(const char *state, uint32_t regionSize)
{
    LOG_I(VFS_LITTLEFS_PORT_LOG_TAG,
          "littlefs mount %s %s offset=0x%08lX size=0x%08lX",
          VFS_LITTLEFS_PORT_MOUNT_PATH,
          state,
          (unsigned long)VFS_LITTLEFS_PORT_REGION_OFFSET,
          (unsigned long)regionSize);
}

static bool vfsLittlefsPortFormatAfterCorrupt(stVfsMountCfg *mountCfg, uint32_t regionSize)
{
    if (mountCfg == NULL) {
        return false;
    }

    mountCfg->isAutoMount = false;
    if (!vfsRegisterMount(mountCfg)) {
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "format mount register fail err=%u", (unsigned)vfsGetStatus()->lastError);
        return false;
    }

    LOG_I(VFS_LITTLEFS_PORT_LOG_TAG, "littlefs first boot format start");
    if (!vfsFormat(VFS_LITTLEFS_PORT_MOUNT_PATH)) {
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "littlefs first boot format fail err=%u", (unsigned)vfsGetStatus()->lastError);
        return false;
    }

    gVfsLittlefsPortRegistered = true;
    gVfsLittlefsPortReady = true;
    vfsLittlefsPortLogReady("formatted", regionSize);
    return true;
}

bool vfsLittlefsPortInit(void)
{
    stVfsLittlefsCfg lLittlefsCfg;
    stVfsMountCfg lMountCfg;
    const stGd25qxxxInfo *lInfo;
    uint32_t lRegionSize;
    const stVfsStatus *lVfsStatus;

    if (gVfsLittlefsPortReady) {
        return true;
    }

    if (!vfsInit()) {
        return false;
    }

    if (gd25qxxxInit(VFS_LITTLEFS_PORT_FLASH_DEVICE) != GD25QXXX_STATUS_OK) {
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "gd25qxxx init fail");
        return false;
    }

    lInfo = gd25qxxxGetInfo(VFS_LITTLEFS_PORT_FLASH_DEVICE);
    lRegionSize = vfsLittlefsPortGetRegionSize(lInfo);
    if (lRegionSize == 0U) {
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "littlefs region invalid");
        return false;
    }

    lLittlefsCfg.blockDeviceOps = &gVfsLittlefsPortBlockOps;
    lLittlefsCfg.blockDeviceContext = (void *)(uintptr_t)VFS_LITTLEFS_PORT_FLASH_DEVICE;
    lLittlefsCfg.regionOffset = VFS_LITTLEFS_PORT_REGION_OFFSET;
    lLittlefsCfg.regionSizeBytes = lRegionSize;
    lLittlefsCfg.readSize = VFS_LITTLEFS_PORT_READ_SIZE;
    lLittlefsCfg.progSize = VFS_LITTLEFS_PORT_PROG_SIZE;
    lLittlefsCfg.blockSize = VFS_LITTLEFS_PORT_BLOCK_SIZE;
    lLittlefsCfg.cacheSize = VFS_LITTLEFS_PORT_CACHE_SIZE;
    lLittlefsCfg.lookaheadSize = VFS_LITTLEFS_PORT_LOOKAHEAD_SIZE;
    lLittlefsCfg.blockCycles = VFS_LITTLEFS_PORT_BLOCK_CYCLES;
    if (!vfsLittlefsInitContext(&gVfsLittlefsPortContext, &lLittlefsCfg)) {
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "littlefs context init fail");
        return false;
    }

    lMountCfg.mountPath = VFS_LITTLEFS_PORT_MOUNT_PATH;
    lMountCfg.backendOps = vfsLittlefsGetBackendOps();
    lMountCfg.backendContext = &gVfsLittlefsPortContext;
    lMountCfg.isAutoMount = true;
    lMountCfg.isReadOnly = false;
    if (!vfsRegisterMount(&lMountCfg)) {
        lVfsStatus = vfsGetStatus();
        if ((lVfsStatus != NULL) && (lVfsStatus->lastError == eVFS_CORRUPT)) {
            return vfsLittlefsPortFormatAfterCorrupt(&lMountCfg, lRegionSize);
        }

        if ((lVfsStatus != NULL) && (lVfsStatus->lastError == eVFS_ALREADY_EXISTS)) {
            if (vfsIsMounted(VFS_LITTLEFS_PORT_MOUNT_PATH)) {
                gVfsLittlefsPortRegistered = true;
                gVfsLittlefsPortReady = true;
                vfsLittlefsPortLogReady("reused", lRegionSize);
                return true;
            }

            if (vfsMount(VFS_LITTLEFS_PORT_MOUNT_PATH)) {
                gVfsLittlefsPortRegistered = true;
                gVfsLittlefsPortReady = true;
                vfsLittlefsPortLogReady("mounted", lRegionSize);
                return true;
            }

            lVfsStatus = vfsGetStatus();
            if ((lVfsStatus != NULL) && (lVfsStatus->lastError == eVFS_CORRUPT)) {
                if (!vfsFormat(VFS_LITTLEFS_PORT_MOUNT_PATH)) {
                    LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "littlefs existing mount format fail err=%u", (unsigned)vfsGetStatus()->lastError);
                    return false;
                }

                gVfsLittlefsPortRegistered = true;
                gVfsLittlefsPortReady = true;
                vfsLittlefsPortLogReady("formatted", lRegionSize);
                return true;
            }

            return false;
        }
        LOG_E(VFS_LITTLEFS_PORT_LOG_TAG, "mount register fail err=%u", (unsigned)vfsGetStatus()->lastError);
        return false;
    }

    gVfsLittlefsPortReady = true;
    vfsLittlefsPortLogReady("ready", lRegionSize);
    gVfsLittlefsPortRegistered = true;
    return true;
}

bool vfsLittlefsPortIsReady(void)
{
    return gVfsLittlefsPortReady;
}

/**************************End of file********************************/
