/************************************************************************************
* @file     : sensor.h
* @brief    : Sensor manager public interface.
* @details  : Samples LIS2HH12 acceleration and force ADC data into an RTOS queue.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SENSOR_H
#define REBUILDCPR_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "lis2hh12.h"
#include "lis2hh12_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_QUEUE_LENGTH                 10U
#define SENSOR_TASK_INTERVAL_MS             10U
#define SENSOR_ACC_SAMPLE_CAPACITY          (FIFO_THRESHOLD_CONFIG + 1U)
#define SENSOR_ACC_AXIS_BUFFER_LENGTH       (ACC_DATA_SAMPLE_PER_INTERVAL * SENSOR_ACC_SAMPLE_CAPACITY)
#define SENSOR_FORCE_ADC_TIMEOUT_MS         10U

typedef struct stSensorSample {
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t force;
} stSensorSample;

bool sensorInit(void);
void sensorProcess(void);
bool sensorReadSample(stSensorSample *sample, uint32_t timeoutMs);
bool sensorIsReady(void);
uint32_t sensorGetDropCount(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
