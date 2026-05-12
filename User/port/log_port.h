/************************************************************************************
* @file     : log_port.h
* @brief    : Project-side log binding entry.
* @details  : Exposes the explicit log ops table consumed by the reusable log
*             core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_LOG_PORT_H
#define REBUILDCPR_LOG_PORT_H

#include "../../rep/sys/log/log.h"

#ifdef __cplusplus
extern "C" {
#endif

const stLogOps *logPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_LOG_PORT_H
/**************************End of file********************************/