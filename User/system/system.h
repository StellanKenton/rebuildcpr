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

typedef enum eSystemMode {
	eSYSTEM_INIT_MODE = 0,
	eSYSTEM_SELF_CHECK_MODE,
	eSYSTEM_STANDBY_MODE,
	eSYSTEM_NORMAL_MODE,
	eSYSTEM_UPDATE_MODE,
	eSYSTEM_DIAGNOSTIC_MODE,
	eSYSTEM_MODE_MAX,
} eSystemMode;

#ifdef __cplusplus
extern "C" {
#endif

bool systemInit(void);
void systemBootstrap(void);
bool systemIsValidMode(eSystemMode mode);
eSystemMode systemGetMode(void);
void systemSetMode(eSystemMode mode);
void systemSyncIndicators(void);
void systemDefaultTaskStep(void);
#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
