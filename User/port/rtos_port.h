/************************************************************************************
* @file     : rtos_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_RTOS_PORT_H
#define REBUILDCPR_RTOS_PORT_H

#include <stdint.h>

#include "rtos.h"

#ifdef __cplusplus
extern "C" {
#endif

const stRepRtosOps *rtosPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
