/************************************************************************************
* @file     : mpu6050_port.h
* @brief    : Project-side MPU6050 binding entry.
* @details  : Exposes the explicit ops table consumed by the reusable MPU6050
*             module core.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MPU6050_PORT_H
#define REBUILDCPR_MPU6050_PORT_H

#include "../../rep/module/mpu6050/mpu6050_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

const stMpu6050Ops *mpu6050PortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_MPU6050_PORT_H
/**************************End of file********************************/