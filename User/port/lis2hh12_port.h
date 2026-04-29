/************************************************************************************
* @file     : lis2hh12_port.h
* @brief    : Project-side LIS2HH12 port configuration.
* @details  : Binds the LIS2HH12 module to the hardware IIC driver.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_LIS2HH12_PORT_H
#define REBUILDCPR_LIS2HH12_PORT_H

#include "lis2hh12_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACC_COMMUNICATE_MAX_RETRY          10U
#define FIFO_THRESHOLD_CONFIG              10U
#define ACC_DATA_SAMPLE_PER_INTERVAL       3U
#define LIS2HH12_PORT_RETRY_DELAY_MS       10U
#define LIS2HH12_PORT_RESET_POLL_DELAY_MS  1U

void lis2hh12LoadPlatformDefaultCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
const stLis2hh12IicInterface *lis2hh12GetPlatformIicInterface(eLis2hh12MapType device);
bool lis2hh12PlatformIsValidAssemble(eLis2hh12MapType device);
uint8_t lis2hh12PlatformGetLinkId(eLis2hh12MapType device);
uint32_t lis2hh12PlatformGetRetryDelayMs(void);
uint32_t lis2hh12PlatformGetResetPollDelayMs(void);
void lis2hh12PlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
