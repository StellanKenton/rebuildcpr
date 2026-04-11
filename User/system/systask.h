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


#define CommTaskStackSize           2U
#define CommTaskPriority            osPriorityBelowNormal7
#define CommTaskInterval            20U

#define MemoryTaskStackSize         4U
#define MemoryTaskPriority          osPriorityLow5
#define MemoryTaskInterval          100U

#define PowerTaskStackSize          4U
#define PowerTaskPriority           osPriorityBelowNormal
#define PowerTaskInterval           100U

#define WirelessTaskStackSize       4U
#define WirelessTaskPriority        osPriorityBelowNormal7
#define WirelessTaskInterval        50U

#define AudioTaskStackSize          4U
#define AudioTaskPriority           osPriorityLow
#define AudioTaskInterval           20U

#define BackgroundTaskStackSize     4U
#define BackgroundTaskPriority      osPriorityBelowNormal
#define BackgroundTaskInterval      5U

#define SystemTaskInterval          50U



bool systaskCreateWorkerTasks(void);
bool systaskCreateBackgroundTask(void);
void systaskRunSystemTask(void *argument);
void systaskRunBackgroundTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
