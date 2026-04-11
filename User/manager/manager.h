/************************************************************************************
* @file     : manager.h
* @brief    : Project task manager aggregation.
* @details  : Declares the worker task service entry points used by systask.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MANAGER_H
#define REBUILDCPR_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void commTaskManager(void);
void memoryTaskManager(void);
void powerTaskManager(void);
void wirelessTaskManager(void);
void audioTaskManager(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
