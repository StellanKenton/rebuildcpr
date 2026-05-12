/************************************************************************************
* @file     : drvspi_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVSPI_PORT_H
#define REBUILDCPR_DRVSPI_PORT_H

#include "drvspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvSpiPortMap {
    DRVSPI_BUS0 = 0,
} eDrvSpiPortMap;

#define DRVSPI_DEFAULT_TIMEOUT_MS 100U

const stDrvSpiOps *drvSpiPortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
