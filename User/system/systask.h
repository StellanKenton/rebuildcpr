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
#define MemoryTaskPriority          8U
#define MemoryTaskInterval          100U

#define PowerTaskStackSize          3U
#define PowerTaskPriority           7U
#define PowerTaskInterval           100U

#define WirelessTaskStackSize       40U
#define WirelessTaskPriority        16U
#define WirelessTaskInterval        5U

#define SensorTaskStackSize         7U
#define SensorTaskPriority          23U
#define SensorTaskInterval          10U

#define CprAlgTaskStackSize         4U
#define CprAlgTaskPriority          21U
#define CprAlgTaskInterval          50U

#define AudioTaskStackSize          21U
#define AudioTaskPriority           22U
#define AudioTaskInterval           20U

#define SystemTaskStackSize         40U
#define SystemTaskPriority          17U
#define SystemTaskInterval          10U
void systemTaskEntry(void *argument);


bool systaskCreateWorkerTasks(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
