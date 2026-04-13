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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define CommTaskStackSize           2U
#define CommTaskPriority            23U
#define CommTaskInterval            20U

#define MemoryTaskStackSize         4U
#define MemoryTaskPriority          13U
#define MemoryTaskInterval          100U

#define PowerTaskStackSize          4U
#define PowerTaskPriority           16U
#define PowerTaskInterval           100U

#define WirelessTaskStackSize       4U
#define WirelessTaskPriority        23U
#define WirelessTaskInterval        50U

#define AudioTaskStackSize          4U
#define AudioTaskPriority           8U
#define AudioTaskInterval           20U

#define BackgroundTaskStackSize     8U
#define BackgroundTaskPriority      16U
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
