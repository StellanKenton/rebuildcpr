/************************************************************************************
* @file     : ec800m_port.h
* @brief    : Project-side EC800M binding entry.
* @details  : Exposes the explicit ops table consumed by the reusable EC800M
*             module core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_EC800M_PORT_H
#define REBUILDCPR_EC800M_PORT_H

#include "../../rep/module/ec800m/ec800m_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

const stEc800mOps *ec800mPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_EC800M_PORT_H
/**************************End of file********************************/