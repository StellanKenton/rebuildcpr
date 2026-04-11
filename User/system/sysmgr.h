/************************************************************************************
* @file     : sysmgr.h
* @brief    : System mode scheduler entry.
* @details  : Declares the project-side system manager dispatch function.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SYSMGR_H
#define REBUILDCPR_SYSMGR_H

#include "system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_STRINGIFY_IMPL(value)    #value
#define SYSTEM_STRINGIFY(value)         SYSTEM_STRINGIFY_IMPL(value)

#define FW_VER_MAJOR                    0
#define FW_VER_MINOR                    0
#define FW_VER_PATCH                    1

#define HW_VER_MAJOR                    0
#define HW_VER_MINOR                    0
#define HW_VER_PATCH                    1

#define FIRMWARE_VERSION                "SoftVer" SYSTEM_STRINGIFY(FW_VER_MAJOR) "." SYSTEM_STRINGIFY(FW_VER_MINOR) "." SYSTEM_STRINGIFY(FW_VER_PATCH)
#define HARDWARE_VERSION                "HardVer" SYSTEM_STRINGIFY(HW_VER_MAJOR) "." SYSTEM_STRINGIFY(HW_VER_MINOR) "." SYSTEM_STRINGIFY(HW_VER_PATCH)

void systemManagerRun(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/












