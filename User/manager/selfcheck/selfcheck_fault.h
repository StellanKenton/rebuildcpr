/************************************************************************************
* @file     : selfcheck_fault.h
* @brief    : Self-check fault detection and latch interface.
* @details  : Maintains current and startup-window fault bits for 0x05 reports.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SELFCHECK_FAULT_H
#define REBUILDCPR_SELFCHECK_FAULT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SELF_CHECK_FAULT_PROCESS_INTERVAL_MS          100U
#define SELF_CHECK_FAULT_STARTUP_DURATION_MS          12000U
#define SELF_CHECK_FAULT_PAYLOAD_LEN                  5U

#define SELF_CHECK_FAULT_MODULE_PASS                  0x01U

#define SELF_CHECK_FAULT_CPR_ACC_INIT                 0x02U
#define SELF_CHECK_FAULT_CPR_RTC                      0x04U

#define SELF_CHECK_FAULT_POWER_3V3_HIGH               0x02U
#define SELF_CHECK_FAULT_POWER_3V3_LOW                0x04U
#define SELF_CHECK_FAULT_POWER_5V_HIGH                0x08U
#define SELF_CHECK_FAULT_POWER_5V_LOW                 0x10U
#define SELF_CHECK_FAULT_POWER_DC_HIGH                0x20U

#define SELF_CHECK_FAULT_AUDIO_COMM                   0x02U
#define SELF_CHECK_FAULT_AUDIO_MUSIC_NUM              0x04U

#define SELF_CHECK_FAULT_WIRELESS_INIT                0x02U

#define SELF_CHECK_FAULT_MEMORY_INIT                  0x02U

typedef struct stSelfCheckFaultPayload {
    uint8_t cpr;
    uint8_t power;
    uint8_t audio;
    uint8_t wireless;
    uint8_t memory;
} stSelfCheckFaultPayload;

void selfCheckFaultInit(void);
void selfCheckFaultResetWindow(void);
void selfCheckFaultProcess100ms(void);
bool selfCheckFaultRunStartupWindow(uint32_t durationMs);
void selfCheckFaultSetBits(const stSelfCheckFaultPayload *faultBits);
void selfCheckFaultClearBits(const stSelfCheckFaultPayload *faultBits);
void selfCheckFaultGetCurrentPayload(stSelfCheckFaultPayload *payload);
void selfCheckFaultGetWindowPayload(stSelfCheckFaultPayload *payload);
void selfCheckFaultConsumeWindowPayload(stSelfCheckFaultPayload *payload);
bool selfCheckFaultGetBootRtcTime(uint32_t *timestamp);
bool selfCheckFaultGetRtcTime(uint32_t *timestamp);
bool selfCheckFaultSetRtcTime(uint32_t timestamp);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
