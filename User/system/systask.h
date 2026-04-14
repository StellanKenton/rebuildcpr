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


#define CommTaskStackSize           8U
#define CommTaskPriority            23U
#define CommTaskInterval            20U
static void systemCommTaskEntry(void *argument);

#define MemoryTaskStackSize         8U
#define MemoryTaskPriority          13U
#define MemoryTaskInterval          100U
static void systemMemoryTaskEntry(void *argument);

#define PowerTaskStackSize          8U
#define PowerTaskPriority           16U
#define PowerTaskInterval           100U
static void systemPowerTaskEntry(void *argument);

#define WirelessTaskStackSize       8U
#define WirelessTaskPriority        23U
#define WirelessTaskInterval        50U
static void systemWirelessTaskEntry(void *argument);

#define AudioTaskStackSize          4U
#define AudioTaskPriority           8U
#define AudioTaskInterval           20U
static void systemAudioTaskEntry(void *argument);

#define SystemTaskStackSize          8U
#define SystemTaskPriority           5U
#define SystemTaskInterval           10U
void systaskRunSystemTask(void *argument);


bool systaskCreateWorkerTasks(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
