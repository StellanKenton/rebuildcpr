/***********************************************************************************
* @file     : manager.c
* @brief    : Project task manager aggregation.
* @details  : Provides the build entry expected by the current project files.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "manager.h"

#include "power/power.h"

/**
* @brief : Handle communication task work.
* @param : None
* @return: None
**/
void commTaskManager(void)
{
}

/**
* @brief : Handle memory task work.
* @param : None
* @return: None
**/
void memoryTaskManager(void)
{
}

/**
* @brief : Handle power task work.
* @param : None
* @return: None
**/
void powerTaskManager(void)
{
    powerProcess();
}

/**
* @brief : Handle wireless task work.
* @param : None
* @return: None
**/
void wirelessTaskManager(void)
{
    
}

/**
* @brief : Handle audio task work.
* @param : None
* @return: None
**/
void audioTaskManager(void)
{
}

/**************************End of file********************************/
