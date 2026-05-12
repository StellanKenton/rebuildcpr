/************************************************************************************
* @file     : lsm6_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_LSM6_PORT_H
#define REBUILDCPR_LSM6_PORT_H

#include "../../rep/module/lsm6/lsm6_assembly.h"

#define LSM6_PORT_IIC_DELAY_US            5U
#define LSM6_PORT_RESET_DELAY_MS          20U
#define LSM6_PORT_RESET_POLL_DELAY_MS     2U

#define LSM6_PORT_SDA_GPIO_Port           GPIOB
#define LSM6_PORT_SDA_Pin                 GPIO_PIN_0
#define LSM6_PORT_SCL_GPIO_Port           GPIOB
#define LSM6_PORT_SCL_Pin                 GPIO_PIN_1

#ifdef __cplusplus
extern "C" {
#endif

const stLsm6Ops *lsm6PortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
