/************************************************************************************
* @file     : esp32c5_port.h
* @brief    : Project-side ESP32-C5 binding entry.
* @details  : Exposes the explicit ops table consumed by the reusable ESP32-C5
*             module core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_ESP32C5_PORT_H
#define REBUILDCPR_ESP32C5_PORT_H

#include "../../rep/module/esp32c5/esp32c5_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

const stEsp32c5Ops *esp32c5PortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_ESP32C5_PORT_H
/**************************End of file********************************/