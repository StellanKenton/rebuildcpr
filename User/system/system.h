/************************************************************************************
* @file     : system.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SYSTEM_H
#define REBUILDCPR_SYSTEM_H

#include <stdbool.h>

#include "../../rep/service/system/system.h"

#ifdef __cplusplus
extern "C" {
#endif

bool systemInit(void);
void systemBootstrap(void);
bool systemIsValidMode(eSystemMode mode);
eSystemMode systemGetMode(void);
void systemSetMode(eSystemMode mode);
#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/