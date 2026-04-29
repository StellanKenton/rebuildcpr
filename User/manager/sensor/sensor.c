/***********************************************************************************
* @file     : sensor.c
* @brief    : Sensor manager implementation.
* @details  : Runs a 10 ms acquisition path for acceleration and force ADC samples.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sensor.h"

#include "drvadc.h"
#include "drvadc_port.h"
#include "lis2hh12_port.h"
#include "rtos.h"
#include "../../../rep/service/log/log.h"

#define SENSOR_LOG_TAG                    "sensor"

static stRepRtosQueue gSensorQueue;
static bool gSensorInitialized = false;
static bool gSensorAccReady = false;
static bool gSensorForceReady = false;
static uint32_t gSensorDropCount = 0U;

static bool sensorQueueSample(const stSensorSample *sample)
{
    if (repRtosQueueSend(&gSensorQueue, sample, 0U) == REP_RTOS_STATUS_OK) {
        return true;
    }

    gSensorDropCount++;
    return false;
}

static bool sensorReadForce(uint16_t *force)
{
    return drvAdcReadRawTimeout(DRVADC_FORCE, force, SENSOR_FORCE_ADC_TIMEOUT_MS) == DRV_STATUS_OK;
}

bool sensorInit(void)
{
    eRepRtosStatus lQueueStatus;

    if (gSensorInitialized) {
        return true;
    }

    lQueueStatus = repRtosQueueCreate(&gSensorQueue, sizeof(stSensorSample), SENSOR_QUEUE_LENGTH);
    if (lQueueStatus != REP_RTOS_STATUS_OK) {
        LOG_E(SENSOR_LOG_TAG, "queue init fail status=%d", (int)lQueueStatus);
        return false;
    }

    gSensorForceReady = (drvAdcInit(DRVADC_FORCE) == DRV_STATUS_OK);
    gSensorAccReady = (lis2hh12Init(LIS2HH12_DEV0) == LIS2HH12_STATUS_OK);

    if (!gSensorForceReady || !gSensorAccReady) {
        LOG_E(SENSOR_LOG_TAG, "init fail acc=%d force=%d", (int)gSensorAccReady, (int)gSensorForceReady);
        return false;
    }

    gSensorInitialized = true;
    LOG_I(SENSOR_LOG_TAG, "init ok");
    return true;
}

void sensorProcess(void)
{
    stLis2hh12Sample lAccSamples[SENSOR_ACC_SAMPLE_CAPACITY];
    uint8_t lSampleCount = 0U;
    uint8_t lIndex;
    eDrvStatus lStatus;

    if (!gSensorInitialized || !gSensorAccReady || !gSensorForceReady) {
        return;
    }

    lStatus = lis2hh12ReadFifoSamples(LIS2HH12_DEV0,
                                      lAccSamples,
                                      SENSOR_ACC_SAMPLE_CAPACITY,
                                      &lSampleCount);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return;
    }

    for (lIndex = 0U; lIndex < lSampleCount; lIndex++) {
        stSensorSample lSensorSample;

        lSensorSample.x = lAccSamples[lIndex].x;
        lSensorSample.y = lAccSamples[lIndex].y;
        lSensorSample.z = lAccSamples[lIndex].z;
        lSensorSample.force = 0U;

        if (!sensorReadForce(&lSensorSample.force)) {
            continue;
        }

        (void)sensorQueueSample(&lSensorSample);
    }
}

bool sensorReadSample(stSensorSample *sample, uint32_t timeoutMs)
{
    if ((sample == NULL) || !gSensorQueue.isCreated) {
        return false;
    }

    return repRtosQueueReceive(&gSensorQueue, sample, timeoutMs) == REP_RTOS_STATUS_OK;
}

bool sensorIsReady(void)
{
    return gSensorInitialized && gSensorAccReady && gSensorForceReady;
}

uint32_t sensorGetDropCount(void)
{
    return gSensorDropCount;
}

/**************************End of file********************************/
