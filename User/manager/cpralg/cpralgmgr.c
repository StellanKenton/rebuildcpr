#include "cpralgmgr.h"

#include "CprFeedback_C.h"
#include "../selfcheck/selfcheck_fault.h"
#include "../sensor/sensor.h"
#include "../../port/pca9535_port.h"
#include "../../port/tm1651_port.h"
#include "../../../rep/service/log/log.h"
#include "rtos.h"

#include <stddef.h>

#define CPR_ALG_MGR_INTERVAL_MS 50U
#define CPR_ALG_MGR_RTC_BASE_YEAR 2025U

static const char gCprAlgMgrLogTag[] = "cpralg";
static const uint8_t gCprAlgMgrFreqDisplayMin = 40U;
static const uint8_t gCprAlgMgrFreqDisplayMax = 160U;
static const uint32_t gCprAlgMgrMetricDisplayMs = 2000U;
static const uint32_t gCprAlgMgrIntervalDisplayStartMs = 3000U;
static const uint32_t gCprAlgMgrIntervalDisplayEndMs = 300000U;
static bool sCprAlgMgrHasFirstPress = false;

CPR_Data_Typedef s_CPR_Data = {0};
CPR_Manager_Typedef s_CPR_Manager = {0};
CPR_Alarm_Limit_Typedef s_CPR_Alarm_Limit = {
    .Depth_High_Limit = 60,
    .Depth_Low_Limit = 50,
    .Freq_High_Limit = 120,
    .Freq_Low_Limit = 100,
    .RealseDepth_Low_Limit = 20,
    .Depth_Alarm_Time = 15000,
    .Freq_Alarm_Time = 15000,
    .RealseDepth_Alarm_Time = 15000,
    .Press_Well_Time = 15000,
};

static bool s_CPR_Alg_Initialized = false;

static bool cprAlgMgrIsLeapYear(uint16_t year)
{
    return (((year % 4U) == 0U) && ((year % 100U) != 0U)) || ((year % 400U) == 0U);
}

static void cprAlgMgrTimestampToDateTime(uint32_t timestamp,
                                         uint16_t *year,
                                         uint8_t *month,
                                         uint8_t *day,
                                         uint8_t *hour,
                                         uint8_t *minute,
                                         uint8_t *second)
{
    static const uint8_t lDaysInMonth[2][12] = {
        {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U},
        {31U, 29U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U},
    };
    uint32_t lTotalDays;
    uint16_t lYear;
    uint8_t lMonth;
    uint8_t lLeap;

    if ((year == NULL) || (month == NULL) || (day == NULL) ||
        (hour == NULL) || (minute == NULL) || (second == NULL)) {
        return;
    }

    *second = (uint8_t)(timestamp % 60U);
    timestamp /= 60U;
    *minute = (uint8_t)(timestamp % 60U);
    timestamp /= 60U;
    *hour = (uint8_t)(timestamp % 24U);
    lTotalDays = timestamp / 24U;

    lYear = CPR_ALG_MGR_RTC_BASE_YEAR;
    while (lTotalDays >= (cprAlgMgrIsLeapYear(lYear) ? 366U : 365U)) {
        lTotalDays -= cprAlgMgrIsLeapYear(lYear) ? 366U : 365U;
        lYear++;
    }

    lLeap = cprAlgMgrIsLeapYear(lYear) ? 1U : 0U;
    for (lMonth = 0U; lMonth < 12U; lMonth++) {
        if (lTotalDays < lDaysInMonth[lLeap][lMonth]) {
            break;
        }
        lTotalDays -= lDaysInMonth[lLeap][lMonth];
    }

    *year = lYear;
    *month = (uint8_t)(lMonth + 1U);
    *day = (uint8_t)(lTotalDays + 1U);
}

static void cprAlgMgrLogBootRtcTime(uint32_t bootTimeStamp)
{
    uint16_t lYear;
    uint8_t lMonth;
    uint8_t lDay;
    uint8_t lHour;
    uint8_t lMinute;
    uint8_t lSecond;

    cprAlgMgrTimestampToDateTime(bootTimeStamp, &lYear, &lMonth, &lDay, &lHour, &lMinute, &lSecond);
    LOG_I(gCprAlgMgrLogTag, "boot rtc timestamp=0x%08lX", (unsigned long)bootTimeStamp);
    LOG_I(gCprAlgMgrLogTag,
          "boot rtc date=%04u-%02u-%02u %02u:%02u:%02u",
          (unsigned int)lYear,
          (unsigned int)lMonth,
          (unsigned int)lDay,
          (unsigned int)lHour,
          (unsigned int)lMinute,
          (unsigned int)lSecond);
}

static uint8_t cprAlgMgrClampToU8(int16_t value)
{
    if (value <= 0) {
        return 0U;
    }

    if (value >= 255) {
        return 255U;
    }

    return (uint8_t)value;
}

static uint8_t cprAlgMgrClampFreqForDisplay(uint8_t freq)
{
    if (freq == 0U) {
        return 0U;
    }

    if (freq < gCprAlgMgrFreqDisplayMin) {
        return gCprAlgMgrFreqDisplayMin;
    }

    if (freq > gCprAlgMgrFreqDisplayMax) {
        return gCprAlgMgrFreqDisplayMax;
    }

    return freq;
}

static uint8_t cprAlgMgrGetDepthLedNum(uint8_t depth)
{
    uint8_t lLedNum;

    if (depth == 0U) {
        return 0U;
    }

    lLedNum = (uint8_t)((depth + 9U) / 10U);
    if (lLedNum > PCA9535_PORT_LED_MAX) {
        lLedNum = PCA9535_PORT_LED_MAX;
    }

    return lLedNum;
}

static bool cprAlgMgrIsPressWell(uint8_t depth, uint8_t freq)
{
    return (depth >= s_CPR_Alarm_Limit.Depth_Low_Limit) &&
           (depth < s_CPR_Alarm_Limit.Depth_High_Limit) &&
           (freq >= s_CPR_Alarm_Limit.Freq_Low_Limit) &&
           (freq < s_CPR_Alarm_Limit.Freq_High_Limit);
}

static void cprAlgMgrUpdateInterval(void)
{
    if ((UINT32_MAX - s_CPR_Manager.IntervalTime) >= CPR_ALG_MGR_INTERVAL_MS) {
        s_CPR_Manager.IntervalTime += CPR_ALG_MGR_INTERVAL_MS;
    } else {
        s_CPR_Manager.IntervalTime = UINT32_MAX;
    }
}

static void cprAlgMgrShowMetrics(uint8_t depth, uint8_t freq)
{
    uint8_t lDisplayFreq = cprAlgMgrClampFreqForDisplay(freq);
    uint8_t lDisplayDepth = depth;

    (void)tm1651PortShowNumber3(lDisplayFreq);
    (void)pca9535PortLedLightNum(cprAlgMgrGetDepthLedNum(lDisplayDepth));
}

static void cprAlgMgrLedProcess(void)
{
    uint8_t lDepth;
    uint8_t lFreq;
    uint32_t lIntervalTime;
    bool lPressWell;

    repRtosEnterCritical();
    lDepth = s_CPR_Data.Depth;
    lFreq = s_CPR_Data.Freq;
    lIntervalTime = s_CPR_Manager.IntervalTime;
    repRtosExitCritical();

    lPressWell = cprAlgMgrIsPressWell(lDepth, lFreq);

    if (!sCprAlgMgrHasFirstPress) {
        (void)tm1651PortShowNone();
        (void)pca9535PortLedLightNum(0U);
        (void)pca9535PortLedPressShow(false, false, false);
        return;
    }

    if (lIntervalTime < gCprAlgMgrMetricDisplayMs) {
        cprAlgMgrShowMetrics(lDepth, lFreq);
        (void)pca9535PortLedPressShow(!lPressWell, lPressWell, false);
        return;
    }

    (void)pca9535PortLedLightNum(0U);

    if (lIntervalTime < gCprAlgMgrIntervalDisplayStartMs) {
        (void)tm1651PortShowNone();
        (void)pca9535PortLedPressShow(!lPressWell, lPressWell, false);
        return;
    }

    if (lIntervalTime <= gCprAlgMgrIntervalDisplayEndMs) {
        (void)tm1651PortShowNumber3((uint16_t)(lIntervalTime / 1000U));
        (void)pca9535PortLedPressShow(false, false, true);
        return;
    }

    (void)tm1651PortShowNone();
    (void)pca9535PortLedPressShow(false, false, false);
}

bool cprAlgMgrInit(void)
{
    CPR_ALG_CONFIG lAlgConfig;
    CPR_ALG_ALARM_LIMIT lAlarmLimit;
    uint32_t lBootTimeStamp;

    if (s_CPR_Alg_Initialized) {
        return true;
    }

    lAlgConfig.fs = 100.0f;
    lAlgConfig.gain = 8192.0f;
    lAlgConfig.fs_correction_factor = 1.0f;
    lAlgConfig.gain_correction_factor = 1.0f;

    lAlarmLimit.depth_lower = (int)s_CPR_Alarm_Limit.Depth_Low_Limit;
    lAlarmLimit.depth_upper = (int)s_CPR_Alarm_Limit.Depth_High_Limit;
    lAlarmLimit.rate_lower = (int)s_CPR_Alarm_Limit.Freq_Low_Limit;
    lAlarmLimit.rate_upper = (int)s_CPR_Alarm_Limit.Freq_High_Limit;

    CprFeedback_init(lAlgConfig);
    CprFeedback_set_alarmlimit(lAlarmLimit);

    if (!selfCheckFaultGetBootRtcTime(&lBootTimeStamp)) {
        lBootTimeStamp = cprAlgMgrGetRtcTime();
    }

    repRtosEnterCritical();
    s_CPR_Data.Depth = 0U;
    s_CPR_Data.Freq = 0U;
    s_CPR_Data.RealseDepth = 0U;
    s_CPR_Data.TimeStamp = 0U;
    s_CPR_Data.BootTimeStamp = lBootTimeStamp;
    s_CPR_Manager.IsPressing = false;
    s_CPR_Manager.DataReady = false;
    s_CPR_Manager.IntervalTime = 0U;
    repRtosExitCritical();

    sCprAlgMgrHasFirstPress = false;
    cprAlgMgrLogBootRtcTime(lBootTimeStamp);

    s_CPR_Alg_Initialized = true;
    return true;
}

void cprAlgMgrProcess(void)
{
    stSensorSample lSample;
    CprFeedbackRst lCprRst;
    bool lDataReady = false;

    if (!s_CPR_Alg_Initialized && !cprAlgMgrInit()) {
        return;
    }

    while (sensorReadSample(&lSample, 0U)) {
        CprFeedback_process(lSample.x, lSample.y, lSample.z, (int16_t)lSample.force);
        CprFeedback_get_cpr_rst(&lCprRst);

        repRtosEnterCritical();
        s_CPR_Manager.IsPressing = lCprRst.CprState;

        if (lCprRst.SinglePressUpdated) {
            s_CPR_Data.Depth = cprAlgMgrClampToU8(lCprRst.CPRDepth);
            s_CPR_Data.Freq = cprAlgMgrClampToU8(lCprRst.CPRRate);
            s_CPR_Data.RealseDepth = cprAlgMgrClampToU8(lCprRst.CPRReleaseDepth_Instantaneous);
            s_CPR_Data.TimeStamp = repRtosGetTickMs();
            s_CPR_Manager.IntervalTime = 0U;
            sCprAlgMgrHasFirstPress = true;
            lDataReady = true;
        }

        s_CPR_Manager.DataReady = lDataReady;
        repRtosExitCritical();
    }

    repRtosEnterCritical();
    s_CPR_Manager.DataReady = lDataReady;
    if (!lDataReady) {
        cprAlgMgrUpdateInterval();
    }
    repRtosExitCritical();
}

void cprAlgMgrDisplayProcess(void)
{
    if (!s_CPR_Alg_Initialized && !cprAlgMgrInit()) {
        return;
    }

    cprAlgMgrLedProcess();
}

void cprAlgMgrGetData(CPR_Data_Typedef *data)
{
    if (data == NULL) {
        return;
    }

    repRtosEnterCritical();
    *data = s_CPR_Data;
    repRtosExitCritical();
}

void cprAlgMgrGetManager(CPR_Manager_Typedef *manager)
{
    if (manager == NULL) {
        return;
    }

    repRtosEnterCritical();
    *manager = s_CPR_Manager;
    repRtosExitCritical();
}

uint32_t cprAlgMgrGetRtcTime(void)
{
    uint32_t lTimestamp;

    if (!selfCheckFaultGetRtcTime(&lTimestamp)) {
        lTimestamp = 0U;
    }

    return lTimestamp;
}

bool cprAlgMgrSetRtcTime(uint32_t timestamp)
{
    return selfCheckFaultSetRtcTime(timestamp);
}
