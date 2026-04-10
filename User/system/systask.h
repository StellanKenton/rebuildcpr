/************************************************************************************
* @file     : systask.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SYSTASK_H
#define REBUILDCPR_SYSTASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool systaskCreateWorkerTasks(void);
void systaskRunSystemTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
