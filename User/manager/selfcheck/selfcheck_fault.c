/***********************************************************************************
* @file     : selfcheck_fault.c
* @brief    : Self-check fault detection and latch implementation.
* @details  : Samples product manager states every 100 ms and latches transient
*             faults for wireless 0x05 reporting.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "selfcheck_fault.h"

#include <string.h>

#include "selfcheck.h"
#include "../../../Core/Inc/rtc.h"
#include "../../../rep/sys/rtos/rtos.h"
#include "../audio/audio.h"
#include "../memory/memory.h"
#include "../power/power.h"
#include "../sensor/sensor.h"
#include "../wireless/wireless.h"

#define SELF_CHECK_FAULT_RTC_VALID_MIN_SECONDS        41904000UL
#define SELF_CHECK_FAULT_RTC_WAIT_READY_TIMEOUT_MS    100U

#define SELF_CHECK_FAULT_3V3_HIGH_SET_10MV            POWER_VOLTAGE_TO_10MV(3600U)
#define SELF_CHECK_FAULT_3V3_HIGH_CLEAR_10MV          POWER_VOLTAGE_TO_10MV(3550U)
#define SELF_CHECK_FAULT_3V3_LOW_SET_10MV             POWER_VOLTAGE_TO_10MV(3000U)
#define SELF_CHECK_FAULT_3V3_LOW_CLEAR_10MV           POWER_VOLTAGE_TO_10MV(3050U)
#define SELF_CHECK_FAULT_5V_HIGH_SET_10MV             POWER_VOLTAGE_TO_10MV(5500U)
#define SELF_CHECK_FAULT_5V_HIGH_CLEAR_10MV           POWER_VOLTAGE_TO_10MV(5400U)
#define SELF_CHECK_FAULT_5V_LOW_SET_10MV              POWER_VOLTAGE_TO_10MV(4500U)
#define SELF_CHECK_FAULT_5V_LOW_CLEAR_10MV            POWER_VOLTAGE_TO_10MV(4600U)
#define SELF_CHECK_FAULT_DC_HIGH_SET_10MV             POWER_VOLTAGE_TO_10MV(8000U)
#define SELF_CHECK_FAULT_DC_HIGH_CLEAR_10MV           POWER_VOLTAGE_TO_10MV(7900U)

#define SELF_CHECK_FAULT_DEBOUNCE_FAST_SET            1U
#define SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET          3U
#define SELF_CHECK_FAULT_DEBOUNCE_FAST_CLEAR          1U
#define SELF_CHECK_FAULT_DEBOUNCE_DEFAULT_CLEAR       3U
#define SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR        5U

#define SELF_CHECK_FAULT_AUDIO_MIN_MUSIC_NUM          1U

#define SELF_CHECK_FAULT_CPR_MASK                     (SELF_CHECK_FAULT_CPR_ACC_INIT | SELF_CHECK_FAULT_CPR_RTC)
#define SELF_CHECK_FAULT_POWER_MASK                   (SELF_CHECK_FAULT_POWER_3V3_HIGH | \
                                                       SELF_CHECK_FAULT_POWER_3V3_LOW | \
                                                       SELF_CHECK_FAULT_POWER_5V_HIGH | \
                                                       SELF_CHECK_FAULT_POWER_5V_LOW | \
                                                       SELF_CHECK_FAULT_POWER_DC_HIGH)
#define SELF_CHECK_FAULT_AUDIO_MASK                   (SELF_CHECK_FAULT_AUDIO_COMM | SELF_CHECK_FAULT_AUDIO_MUSIC_NUM)
#define SELF_CHECK_FAULT_WIRELESS_MASK                SELF_CHECK_FAULT_WIRELESS_INIT
#define SELF_CHECK_FAULT_MEMORY_MASK                  SELF_CHECK_FAULT_MEMORY_INIT

typedef struct stSelfCheckFaultDebounce {
    uint8_t activeCount;
    uint8_t clearCount;
    bool active;
} stSelfCheckFaultDebounce;

static stSelfCheckFaultPayload gSelfCheckFaultAuto;
static stSelfCheckFaultPayload gSelfCheckFaultExternal;
static stSelfCheckFaultPayload gSelfCheckFaultCurrent;
static stSelfCheckFaultPayload gSelfCheckFaultWindow;
static uint32_t gSelfCheckFaultLastSampleTick;
static bool gSelfCheckFaultInitialized;
static bool gSelfCheckFaultStartupRunning;
static uint32_t gSelfCheckFaultStartupStartTick;
static uint32_t gSelfCheckFaultBootRtcTime;
static bool gSelfCheckFaultBootRtcReady;

static stSelfCheckFaultDebounce gSelfCheckFaultAccDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultRtcDebounce;
static stSelfCheckFaultDebounce gSelfCheckFault3v3HighDebounce;
static stSelfCheckFaultDebounce gSelfCheckFault3v3LowDebounce;
static stSelfCheckFaultDebounce gSelfCheckFault5vHighDebounce;
static stSelfCheckFaultDebounce gSelfCheckFault5vLowDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultDcHighDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultAudioCommDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultAudioMusicDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultWirelessDebounce;
static stSelfCheckFaultDebounce gSelfCheckFaultMemoryDebounce;

static stSelfCheckFaultPayload selfCheckFaultSanitizePayload(const stSelfCheckFaultPayload *payload);
static void selfCheckFaultOrPayload(stSelfCheckFaultPayload *target, const stSelfCheckFaultPayload *source);
static void selfCheckFaultAndNotPayload(stSelfCheckFaultPayload *target, const stSelfCheckFaultPayload *source);
static stSelfCheckFaultPayload selfCheckFaultMergePayload(const stSelfCheckFaultPayload *left, const stSelfCheckFaultPayload *right);
static stSelfCheckFaultPayload selfCheckFaultBuildPayload(const stSelfCheckFaultPayload *faultBits);
static void selfCheckFaultRefreshCurrentLocked(void);
static void selfCheckFaultResetDebounce(stSelfCheckFaultDebounce *debounce);
static bool selfCheckFaultUpdateDebounce(stSelfCheckFaultDebounce *debounce,
                                         bool faultDetected,
                                         uint8_t activeSamples,
                                         uint8_t clearSamples);
static bool selfCheckFaultIsRtcReady(void);
static bool selfCheckFaultReadRtcCounter(uint32_t *counter);
static bool selfCheckFaultIsRtcTimeValid(void);
static void selfCheckFaultCaptureBootRtcTime(void);
static bool selfCheckFaultIsHighVoltageFault(uint16_t voltage10Mv, bool active, uint16_t set10Mv, uint16_t clear10Mv);
static bool selfCheckFaultIsLowVoltageFault(uint16_t voltage10Mv, bool active, uint16_t set10Mv, uint16_t clear10Mv);
static stSelfCheckFaultPayload selfCheckFaultCollectAutoFaults(void);

static stSelfCheckFaultPayload selfCheckFaultSanitizePayload(const stSelfCheckFaultPayload *payload)
{
    stSelfCheckFaultPayload lResult;

    (void)memset(&lResult, 0, sizeof(lResult));
    if (payload == NULL) {
        return lResult;
    }

    lResult.cpr = (uint8_t)(payload->cpr & SELF_CHECK_FAULT_CPR_MASK);
    lResult.power = (uint8_t)(payload->power & SELF_CHECK_FAULT_POWER_MASK);
    lResult.audio = (uint8_t)(payload->audio & SELF_CHECK_FAULT_AUDIO_MASK);
    lResult.wireless = (uint8_t)(payload->wireless & SELF_CHECK_FAULT_WIRELESS_MASK);
    lResult.memory = (uint8_t)(payload->memory & SELF_CHECK_FAULT_MEMORY_MASK);
    return lResult;
}

static void selfCheckFaultOrPayload(stSelfCheckFaultPayload *target, const stSelfCheckFaultPayload *source)
{
    if ((target == NULL) || (source == NULL)) {
        return;
    }

    target->cpr |= source->cpr;
    target->power |= source->power;
    target->audio |= source->audio;
    target->wireless |= source->wireless;
    target->memory |= source->memory;
}

static void selfCheckFaultAndNotPayload(stSelfCheckFaultPayload *target, const stSelfCheckFaultPayload *source)
{
    if ((target == NULL) || (source == NULL)) {
        return;
    }

    target->cpr &= (uint8_t)(~source->cpr);
    target->power &= (uint8_t)(~source->power);
    target->audio &= (uint8_t)(~source->audio);
    target->wireless &= (uint8_t)(~source->wireless);
    target->memory &= (uint8_t)(~source->memory);
}

static stSelfCheckFaultPayload selfCheckFaultMergePayload(const stSelfCheckFaultPayload *left, const stSelfCheckFaultPayload *right)
{
    stSelfCheckFaultPayload lResult;

    (void)memset(&lResult, 0, sizeof(lResult));
    if (left != NULL) {
        lResult = *left;
    }
    selfCheckFaultOrPayload(&lResult, right);
    return lResult;
}

static uint8_t selfCheckFaultFinalizeModuleByte(uint8_t faultBits)
{
    return (faultBits == 0U) ? SELF_CHECK_FAULT_MODULE_PASS : faultBits;
}

static stSelfCheckFaultPayload selfCheckFaultBuildPayload(const stSelfCheckFaultPayload *faultBits)
{
    stSelfCheckFaultPayload lFaultBits = selfCheckFaultSanitizePayload(faultBits);
    stSelfCheckFaultPayload lPayload;

    lPayload.cpr = selfCheckFaultFinalizeModuleByte(lFaultBits.cpr);
    lPayload.power = selfCheckFaultFinalizeModuleByte(lFaultBits.power);
    lPayload.audio = selfCheckFaultFinalizeModuleByte(lFaultBits.audio);
    lPayload.wireless = selfCheckFaultFinalizeModuleByte(lFaultBits.wireless);
    lPayload.memory = selfCheckFaultFinalizeModuleByte(lFaultBits.memory);
    return lPayload;
}

static void selfCheckFaultRefreshCurrentLocked(void)
{
    gSelfCheckFaultCurrent = selfCheckFaultMergePayload(&gSelfCheckFaultAuto, &gSelfCheckFaultExternal);
}

static void selfCheckFaultResetDebounce(stSelfCheckFaultDebounce *debounce)
{
    if (debounce == NULL) {
        return;
    }

    debounce->activeCount = 0U;
    debounce->clearCount = 0U;
    debounce->active = false;
}

static bool selfCheckFaultUpdateDebounce(stSelfCheckFaultDebounce *debounce,
                                         bool faultDetected,
                                         uint8_t activeSamples,
                                         uint8_t clearSamples)
{
    if (debounce == NULL) {
        return false;
    }

    if (activeSamples == 0U) {
        activeSamples = 1U;
    }
    if (clearSamples == 0U) {
        clearSamples = 1U;
    }

    if (debounce->active) {
        if (faultDetected) {
            debounce->clearCount = 0U;
            debounce->activeCount = activeSamples;
        } else {
            debounce->activeCount = 0U;
            if (debounce->clearCount < clearSamples) {
                debounce->clearCount++;
            }
            if (debounce->clearCount >= clearSamples) {
                debounce->active = false;
                debounce->clearCount = 0U;
            }
        }
    } else {
        if (faultDetected) {
            debounce->clearCount = 0U;
            if (debounce->activeCount < activeSamples) {
                debounce->activeCount++;
            }
            if (debounce->activeCount >= activeSamples) {
                debounce->active = true;
                debounce->activeCount = activeSamples;
            }
        } else {
            debounce->activeCount = 0U;
            debounce->clearCount = clearSamples;
        }
    }

    return debounce->active;
}

static bool selfCheckFaultIsRtcReady(void)
{
    uint32_t lStartTick = repRtosGetTickMs();

    if (hrtc.Instance == NULL) {
        return false;
    }

    while ((hrtc.Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET) {
        if ((uint32_t)(repRtosGetTickMs() - lStartTick) >= SELF_CHECK_FAULT_RTC_WAIT_READY_TIMEOUT_MS) {
            return false;
        }
    }

    return true;
}

static bool selfCheckFaultReadRtcCounter(uint32_t *counter)
{
    uint16_t lHigh1;
    uint16_t lHigh2;
    uint16_t lLow;

    if ((counter == NULL) || !selfCheckFaultIsRtcReady()) {
        return false;
    }

    repRtosEnterCritical();
    lHigh1 = (uint16_t)READ_REG(hrtc.Instance->CNTH);
    lLow = (uint16_t)READ_REG(hrtc.Instance->CNTL);
    lHigh2 = (uint16_t)READ_REG(hrtc.Instance->CNTH);
    if (lHigh1 != lHigh2) {
        lLow = (uint16_t)READ_REG(hrtc.Instance->CNTL);
    }
    repRtosExitCritical();

    *counter = (((uint32_t)lHigh2 << 16U) | (uint32_t)lLow);
    return true;
}

bool selfCheckFaultGetRtcTime(uint32_t *timestamp)
{
    return selfCheckFaultReadRtcCounter(timestamp);
}

bool selfCheckFaultSetRtcTime(uint32_t timestamp)
{
    if (!selfCheckFaultIsRtcReady()) {
        return false;
    }

    repRtosEnterCritical();
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
    WRITE_REG(hrtc.Instance->CNTH, (timestamp >> 16U));
    WRITE_REG(hrtc.Instance->CNTL, (timestamp & RTC_CNTL_RTC_CNT));
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
    repRtosExitCritical();

    return selfCheckFaultIsRtcReady();
}

static bool selfCheckFaultIsRtcTimeValid(void)
{
    uint32_t lCounter;

    if (!selfCheckFaultReadRtcCounter(&lCounter)) {
        return false;
    }

    return lCounter >= SELF_CHECK_FAULT_RTC_VALID_MIN_SECONDS;
}

static void selfCheckFaultCaptureBootRtcTime(void)
{
    uint32_t lCounter;

    if (!selfCheckFaultReadRtcCounter(&lCounter)) {
        repRtosEnterCritical();
        gSelfCheckFaultBootRtcTime = 0U;
        gSelfCheckFaultBootRtcReady = false;
        repRtosExitCritical();
        return;
    }

    repRtosEnterCritical();
    gSelfCheckFaultBootRtcTime = lCounter;
    gSelfCheckFaultBootRtcReady = true;
    repRtosExitCritical();
}

static bool selfCheckFaultIsHighVoltageFault(uint16_t voltage10Mv, bool active, uint16_t set10Mv, uint16_t clear10Mv)
{
    return active ? (voltage10Mv >= clear10Mv) : (voltage10Mv > set10Mv);
}

static bool selfCheckFaultIsLowVoltageFault(uint16_t voltage10Mv, bool active, uint16_t set10Mv, uint16_t clear10Mv)
{
    return active ? (voltage10Mv <= clear10Mv) : (voltage10Mv < set10Mv);
}

static stSelfCheckFaultPayload selfCheckFaultCollectAutoFaults(void)
{
    stSelfCheckFaultPayload lFaults;
    stSensorInitStatus lSensorStatus;
    const PowerManager *lPowerManager;
    const stAudioStatus *lAudioStatus;
    const eWirelessState *lWirelessState;
    const stSelfCheckSummary *lSelfCheckSummary;

    (void)memset(&lFaults, 0, sizeof(lFaults));
    (void)memset(&lSensorStatus, 0, sizeof(lSensorStatus));
    sensorGetInitStatus(&lSensorStatus);
    if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultAccDebounce,
                                     !lSensorStatus.accReady,
                                     SELF_CHECK_FAULT_DEBOUNCE_FAST_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_DEFAULT_CLEAR)) {
        lFaults.cpr |= SELF_CHECK_FAULT_CPR_ACC_INIT;
    }

    if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultRtcDebounce,
                                     !selfCheckFaultIsRtcTimeValid(),
                                     SELF_CHECK_FAULT_DEBOUNCE_FAST_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_FAST_CLEAR)) {
        lFaults.cpr |= SELF_CHECK_FAULT_CPR_RTC;
    }

    lPowerManager = powerGetManager();
    if (lPowerManager != NULL) {
        if (selfCheckFaultUpdateDebounce(&gSelfCheckFault3v3HighDebounce,
                                         selfCheckFaultIsHighVoltageFault(lPowerManager->voltage.v3v3Mv,
                                                                         gSelfCheckFault3v3HighDebounce.active,
                                                                         SELF_CHECK_FAULT_3V3_HIGH_SET_10MV,
                                                                         SELF_CHECK_FAULT_3V3_HIGH_CLEAR_10MV),
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
            lFaults.power |= SELF_CHECK_FAULT_POWER_3V3_HIGH;
        }
        if (selfCheckFaultUpdateDebounce(&gSelfCheckFault3v3LowDebounce,
                                         selfCheckFaultIsLowVoltageFault(lPowerManager->voltage.v3v3Mv,
                                                                        gSelfCheckFault3v3LowDebounce.active,
                                                                        SELF_CHECK_FAULT_3V3_LOW_SET_10MV,
                                                                        SELF_CHECK_FAULT_3V3_LOW_CLEAR_10MV),
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
            lFaults.power |= SELF_CHECK_FAULT_POWER_3V3_LOW;
        }
        if (selfCheckFaultUpdateDebounce(&gSelfCheckFault5vHighDebounce,
                                         selfCheckFaultIsHighVoltageFault(lPowerManager->voltage.v5v0Mv,
                                                                         gSelfCheckFault5vHighDebounce.active,
                                                                         SELF_CHECK_FAULT_5V_HIGH_SET_10MV,
                                                                         SELF_CHECK_FAULT_5V_HIGH_CLEAR_10MV),
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
            lFaults.power |= SELF_CHECK_FAULT_POWER_5V_HIGH;
        }
        if (selfCheckFaultUpdateDebounce(&gSelfCheckFault5vLowDebounce,
                                         selfCheckFaultIsLowVoltageFault(lPowerManager->voltage.v5v0Mv,
                                                                        gSelfCheckFault5vLowDebounce.active,
                                                                        SELF_CHECK_FAULT_5V_LOW_SET_10MV,
                                                                        SELF_CHECK_FAULT_5V_LOW_CLEAR_10MV),
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
            lFaults.power |= SELF_CHECK_FAULT_POWER_5V_LOW;
        }
        if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultDcHighDebounce,
                                         selfCheckFaultIsHighVoltageFault(lPowerManager->voltage.dcMv,
                                                                         gSelfCheckFaultDcHighDebounce.active,
                                                                         SELF_CHECK_FAULT_DC_HIGH_SET_10MV,
                                                                         SELF_CHECK_FAULT_DC_HIGH_CLEAR_10MV),
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                         SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
            lFaults.power |= SELF_CHECK_FAULT_POWER_DC_HIGH;
        }
    }

    lAudioStatus = audioGetStatus();
    if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultAudioCommDebounce,
                                     (lAudioStatus == NULL) ||
                                     (lAudioStatus->state == AUDIO_STATE_FAULT) ||
                                     !lAudioStatus->commResponded,
                                     SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
        lFaults.audio |= SELF_CHECK_FAULT_AUDIO_COMM;
    }
    if ((lAudioStatus != NULL) &&
        (lAudioStatus->state == AUDIO_STATE_READY) &&
        lAudioStatus->moduleReady &&
        lAudioStatus->musicNumValid &&
        selfCheckFaultUpdateDebounce(&gSelfCheckFaultAudioMusicDebounce,
                                     lAudioStatus->musicNum < SELF_CHECK_FAULT_AUDIO_MIN_MUSIC_NUM,
                                     SELF_CHECK_FAULT_DEBOUNCE_FAST_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_DEFAULT_CLEAR)) {
        lFaults.audio |= SELF_CHECK_FAULT_AUDIO_MUSIC_NUM;
    } else {
        (void)selfCheckFaultUpdateDebounce(&gSelfCheckFaultAudioMusicDebounce,
                                           false,
                                           SELF_CHECK_FAULT_DEBOUNCE_FAST_SET,
                                           SELF_CHECK_FAULT_DEBOUNCE_DEFAULT_CLEAR);
    }

    lWirelessState = wirelessGetStatus();
    if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultWirelessDebounce,
                                     (lWirelessState == NULL) || (*lWirelessState == eWIRELESS_STATE_ERROR),
                                     SELF_CHECK_FAULT_DEBOUNCE_STABLE_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_STABLE_CLEAR)) {
        lFaults.wireless |= SELF_CHECK_FAULT_WIRELESS_INIT;
    }

    lSelfCheckSummary = selfCheckGetSummary();
    if (selfCheckFaultUpdateDebounce(&gSelfCheckFaultMemoryDebounce,
                                     (lSelfCheckSummary == NULL) || !lSelfCheckSummary->flashReady || !memoryIsReady(),
                                     SELF_CHECK_FAULT_DEBOUNCE_FAST_SET,
                                     SELF_CHECK_FAULT_DEBOUNCE_DEFAULT_CLEAR)) {
        lFaults.memory |= SELF_CHECK_FAULT_MEMORY_INIT;
    }

    return lFaults;
}

void selfCheckFaultInit(void)
{
    repRtosEnterCritical();
    (void)memset(&gSelfCheckFaultAuto, 0, sizeof(gSelfCheckFaultAuto));
    (void)memset(&gSelfCheckFaultExternal, 0, sizeof(gSelfCheckFaultExternal));
    (void)memset(&gSelfCheckFaultCurrent, 0, sizeof(gSelfCheckFaultCurrent));
    (void)memset(&gSelfCheckFaultWindow, 0, sizeof(gSelfCheckFaultWindow));
    gSelfCheckFaultLastSampleTick = 0U;
    gSelfCheckFaultStartupRunning = false;
    gSelfCheckFaultStartupStartTick = 0U;
    gSelfCheckFaultBootRtcTime = 0U;
    gSelfCheckFaultBootRtcReady = false;
    gSelfCheckFaultInitialized = true;
    repRtosExitCritical();

    selfCheckFaultResetDebounce(&gSelfCheckFaultAccDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultRtcDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFault3v3HighDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFault3v3LowDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFault5vHighDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFault5vLowDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultDcHighDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultAudioCommDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultAudioMusicDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultWirelessDebounce);
    selfCheckFaultResetDebounce(&gSelfCheckFaultMemoryDebounce);
}

void selfCheckFaultResetWindow(void)
{
    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    gSelfCheckFaultWindow = gSelfCheckFaultCurrent;
    gSelfCheckFaultLastSampleTick = 0U;
    repRtosExitCritical();
}

void selfCheckFaultProcess100ms(void)
{
    uint32_t lNowTick;
    stSelfCheckFaultPayload lAutoFaults;

    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    lNowTick = repRtosGetTickMs();
    if ((gSelfCheckFaultLastSampleTick != 0U) &&
        ((uint32_t)(lNowTick - gSelfCheckFaultLastSampleTick) < SELF_CHECK_FAULT_PROCESS_INTERVAL_MS)) {
        return;
    }
    gSelfCheckFaultLastSampleTick = lNowTick;

    lAutoFaults = selfCheckFaultCollectAutoFaults();
    repRtosEnterCritical();
    gSelfCheckFaultAuto = lAutoFaults;
    selfCheckFaultRefreshCurrentLocked();
    selfCheckFaultOrPayload(&gSelfCheckFaultWindow, &gSelfCheckFaultCurrent);
    repRtosExitCritical();
}

bool selfCheckFaultRunStartupWindow(uint32_t durationMs)
{
    uint32_t lNowTick;

    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    lNowTick = repRtosGetTickMs();
    if (!gSelfCheckFaultStartupRunning) {
        selfCheckFaultResetWindow();
        selfCheckFaultCaptureBootRtcTime();
        gSelfCheckFaultStartupStartTick = lNowTick;
        gSelfCheckFaultStartupRunning = true;
    }

    selfCheckFaultProcess100ms();
    if ((uint32_t)(lNowTick - gSelfCheckFaultStartupStartTick) < durationMs) {
        return false;
    }

    gSelfCheckFaultStartupRunning = false;
    return true;
}

void selfCheckFaultSetBits(const stSelfCheckFaultPayload *faultBits)
{
    stSelfCheckFaultPayload lFaultBits = selfCheckFaultSanitizePayload(faultBits);

    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    selfCheckFaultOrPayload(&gSelfCheckFaultExternal, &lFaultBits);
    selfCheckFaultRefreshCurrentLocked();
    selfCheckFaultOrPayload(&gSelfCheckFaultWindow, &lFaultBits);
    repRtosExitCritical();
}

void selfCheckFaultClearBits(const stSelfCheckFaultPayload *faultBits)
{
    stSelfCheckFaultPayload lFaultBits = selfCheckFaultSanitizePayload(faultBits);

    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    selfCheckFaultAndNotPayload(&gSelfCheckFaultExternal, &lFaultBits);
    selfCheckFaultRefreshCurrentLocked();
    repRtosExitCritical();
}

void selfCheckFaultGetCurrentPayload(stSelfCheckFaultPayload *payload)
{
    stSelfCheckFaultPayload lCurrent;

    if (payload == NULL) {
        return;
    }
    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    lCurrent = gSelfCheckFaultCurrent;
    repRtosExitCritical();
    *payload = selfCheckFaultBuildPayload(&lCurrent);
}

void selfCheckFaultGetWindowPayload(stSelfCheckFaultPayload *payload)
{
    stSelfCheckFaultPayload lWindow;

    if (payload == NULL) {
        return;
    }
    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    lWindow = selfCheckFaultMergePayload(&gSelfCheckFaultWindow, &gSelfCheckFaultCurrent);
    repRtosExitCritical();
    *payload = selfCheckFaultBuildPayload(&lWindow);
}

void selfCheckFaultConsumeWindowPayload(stSelfCheckFaultPayload *payload)
{
    stSelfCheckFaultPayload lWindow;

    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    lWindow = selfCheckFaultMergePayload(&gSelfCheckFaultWindow, &gSelfCheckFaultCurrent);
    gSelfCheckFaultWindow = gSelfCheckFaultCurrent;
    repRtosExitCritical();

    if (payload != NULL) {
        *payload = selfCheckFaultBuildPayload(&lWindow);
    }
}

bool selfCheckFaultGetBootRtcTime(uint32_t *timestamp)
{
    bool lReady;

    if (timestamp == NULL) {
        return false;
    }
    if (!gSelfCheckFaultInitialized) {
        selfCheckFaultInit();
    }

    repRtosEnterCritical();
    *timestamp = gSelfCheckFaultBootRtcTime;
    lReady = gSelfCheckFaultBootRtcReady;
    repRtosExitCritical();
    return lReady;
}

/**************************End of file********************************/
