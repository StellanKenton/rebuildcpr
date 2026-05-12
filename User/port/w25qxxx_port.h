/************************************************************************************
* @file     : w25qxxx_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_W25QXXX_PORT_H
#define REBUILDCPR_W25QXXX_PORT_H

#include "../../rep/module/w25qxxx/w25qxxx.h"

#ifdef __cplusplus
extern "C" {
#endif

const stW25qxxxOps *w25qxxxPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
