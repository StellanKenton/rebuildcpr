/************************************************************************************
* @file     : fc41dport.h
* @brief    : Project-side FC41D binding helpers.
* @details  : Exposes the explicit FC41D ops table consumed by the reusable
*             module core.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_FC41DPORT_H
#define REBUILDCPR_FC41DPORT_H

#include <stdint.h>

#include "../../rep/module/fc41d/fc41d_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

const stFc41dOps *fc41dPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
