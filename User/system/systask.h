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


#define MemoryTaskStackSize         24U
#define MemoryTaskPriority          13U
#define MemoryTaskInterval          100U

#define PowerTaskStackSize          8U
#define PowerTaskPriority           16U
#define PowerTaskInterval           100U

#define WirelessTaskStackSize       24U
#define WirelessTaskPriority        23U
#define WirelessTaskInterval        5U

#define SensorTaskStackSize         8U
#define SensorTaskPriority          18U
#define SensorTaskInterval          10U

#define CprAlgTaskStackSize         16U
#define CprAlgTaskPriority          17U
#define CprAlgTaskInterval          50U

#define AudioTaskStackSize          4U
#define AudioTaskPriority           8U
#define AudioTaskInterval           20U

#define SystemTaskStackSize         40U
#define SystemTaskPriority          5U
#define SystemTaskInterval          10U
void systemTaskEntry(void *argument);


bool systaskCreateWorkerTasks(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
