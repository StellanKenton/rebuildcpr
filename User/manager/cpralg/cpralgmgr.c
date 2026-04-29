#include "cpralgmgr.h"

#include "CprFeedback_C.h"
#include "../sensor/sensor.h"
#include "rtos.h"

#include <stddef.h>

#define CPR_ALG_MGR_INTERVAL_MS 50U

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

static void cprAlgMgrUpdateInterval(void)
{
    if ((s_CPR_Data.Depth == 0U) || (s_CPR_Data.Freq == 0U)) {
        if ((UINT32_MAX - s_CPR_Manager.IntervalTime) >= CPR_ALG_MGR_INTERVAL_MS) {
            s_CPR_Manager.IntervalTime += CPR_ALG_MGR_INTERVAL_MS;
        } else {
            s_CPR_Manager.IntervalTime = UINT32_MAX;
        }
    }
}

bool cprAlgMgrInit(void)
{
    CPR_ALG_CONFIG lAlgConfig;
    CPR_ALG_ALARM_LIMIT lAlarmLimit;

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

    repRtosEnterCritical();
    s_CPR_Data.Depth = 0U;
    s_CPR_Data.Freq = 0U;
    s_CPR_Data.RealseDepth = 0U;
    s_CPR_Data.TimeStamp = 0U;
    s_CPR_Manager.IsPressing = false;
    s_CPR_Manager.DataReady = false;
    s_CPR_Manager.BootTimeStamp = repRtosGetTickMs();
    s_CPR_Manager.IntervalTime = 0U;
    repRtosExitCritical();

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
            lDataReady = true;
        }

        s_CPR_Manager.DataReady = lDataReady;
        repRtosExitCritical();
    }

    repRtosEnterCritical();
    if (!lDataReady) {
        s_CPR_Manager.DataReady = false;
    }
    cprAlgMgrUpdateInterval();
    repRtosExitCritical();
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
