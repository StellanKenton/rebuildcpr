/***********************************************************************************
* @file     : system_debug.c
* @brief    : System debug and console command implementation.
* @details  : This file hosts optional console bindings for system debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system_debug.h"

#include <stdint.h>
#include <string.h>

#include "../../Core/Inc/main.h"
#include "../../rep/driver/drvadc/drvadc_debug.h"
#include "../../rep/driver/drvanlogiic/drvanlogiic_debug.h"
#include "../../rep/driver/drvgpio/drvgpio_debug.h"
#include "../../rep/driver/drviic/drviic_debug.h"
#include "../../rep/driver/drvspi/drvspi_debug.h"
#include "../../rep/driver/drvuart/drvuart_debug.h"
#include "../../rep/driver/drvadc/drvadc.h"
#include "../../rep/service/log/console.h"
#include "../../rep/service/log/log.h"
#include "../../rep/module/lis2hh12/lis2hh12.h"
#include "../manager/memory/memory_debug.h"
#include "../manager/iotmanager/protcolmgr_debug.h"
#include "../manager/power/power.h"
#include "../manager/sensor/sensor.h"
#include "../manager/wireless/wireless.h"
#include "../port/drvadc_port.h"
#include "../port/lis2hh12_port.h"
#include "../rep_config.h"
#include "system.h"
#include "systask.h"

#define SYSTEM_DEBUG_LOG_TAG "system_debug"

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"

#define SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS            16U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS     50U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT         20U
#define SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 4U)
#define SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)

typedef struct {
    TaskStatus_t previousTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    TaskStatus_t currentTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    UBaseType_t previousTaskCount;
    UBaseType_t currentTaskCount;
    uint32_t previousTotalRunTime;
    uint32_t currentTotalRunTime;
} stSystemDebugTaskUsageContext;

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle);
static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime);
static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta);
static void systemDebugTaskUsageSampler(void *parameter);
#endif

static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleWirelessHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleSensorHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsolePowerDebugHandler(uint32_t transport, int argc, char *argv[]);
static bool gSystemDebugBackgroundServicesReady = false;

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleTaskStackHandler(uint32_t transport, int argc, char *argv[]);

static uint32_t systemDebugBytesToWords(uint32_t sizeBytes);
static uint32_t systemDebugRoundUpDivide(uint32_t dividend, uint32_t divisor);
static bool systemDebugIsSameTaskName(const char *taskName, const char *expectedName);
static bool systemDebugResolveTaskStackConfig(const char *taskName,
    uint32_t usedBytes,
    uint32_t *currentBytes,
    uint32_t *minBytes,
    uint32_t *applyBytes,
    const char **configName,
    const char **configUnitText);

static TaskHandle_t gSystemDebugTaskUsageHandle = NULL;
static stSystemDebugTaskUsageContext gSystemDebugTaskUsageContext;
#endif

static const stConsoleCommand gSystemVersionConsoleCommand = {
    .commandName = "ver",
    .helpText = "ver - show firmware and hardware version",
    .ownerTag = "system",
    .handler = systemDebugConsoleVersionHandler,
};

static const stConsoleCommand gSystemStatusConsoleCommand = {
    .commandName = "sys",
    .helpText = "sys - show system status",
    .ownerTag = "system",
    .handler = systemDebugConsoleStatusHandler,
};

static const stConsoleCommand gSystemRebootConsoleCommand = {
    .commandName = "reboot",
    .helpText = "reboot - software reset the MCU",
    .ownerTag = "system",
    .handler = systemDebugConsoleRebootHandler,
};

static const stConsoleCommand gSystemWirelessConsoleCommand = {
    .commandName = "wireless",
    .helpText = "wireless <status|ble_on|ble_off|wifi_on|wifi_off|wifi_connect|mqtt_on|mqtt_off>",
    .ownerTag = "wireless",
    .handler = systemDebugConsoleWirelessHandler,
};

static const stConsoleCommand gSystemSensorConsoleCommand = {
    .commandName = "sensor",
    .helpText = "sensor - read accelerometer x/y/z and force ADC raw values",
    .ownerTag = "sensor",
    .handler = systemDebugConsoleSensorHandler,
};

static const stConsoleCommand gSystemPowerDebugConsoleCommand = {
    .commandName = "power",
    .helpText = "power - show current power real voltages in 10mV and battery level",
    .ownerTag = "power",
    .handler = systemDebugConsolePowerDebugHandler,
};

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static const stConsoleCommand gSystemTaskUsageConsoleCommand = {
    .commandName = "top",
    .helpText = "top - sample task cpu usage every 50 ms for 1 s",
    .ownerTag = "system",
    .handler = systemDebugConsoleTaskUsageHandler,
};

static const stConsoleCommand gSystemTaskStackConsoleCommand = {
    .commandName = "stack",
    .helpText = "stack - show task stack high-water mark and recommended stack config",
    .ownerTag = "system",
    .handler = systemDebugConsoleTaskStackHandler,
};

static uint32_t systemDebugBytesToWords(uint32_t sizeBytes)
{
    return sizeBytes / (uint32_t)sizeof(StackType_t);
}

static uint32_t systemDebugRoundUpDivide(uint32_t dividend, uint32_t divisor)
{
    if (divisor == 0U) {
        return 0U;
    }

    return (dividend + divisor - 1U) / divisor;
}

static bool systemDebugIsSameTaskName(const char *taskName, const char *expectedName)
{
    if ((taskName == NULL) || (expectedName == NULL)) {
        return false;
    }

    return strcmp(taskName, expectedName) == 0;
}

static bool systemDebugResolveTaskStackConfig(const char *taskName,
    uint32_t usedBytes,
    uint32_t *currentBytes,
    uint32_t *minBytes,
    uint32_t *applyBytes,
    const char **configName,
    const char **configUnitText)
{
    uint32_t lCurrentBytes = 0U;
    uint32_t lConfigQuantumBytes = 0U;
    const char *lConfigName = NULL;
    const char *lConfigUnitText = NULL;

    if ((taskName == NULL) ||
        (currentBytes == NULL) ||
        (minBytes == NULL) ||
        (applyBytes == NULL) ||
        (configName == NULL) ||
        (configUnitText == NULL)) {
        return false;
    }

    if (systemDebugIsSameTaskName(taskName, "systemTask")) {
        lCurrentBytes = 128U * SystemTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "SystemTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "memorytask")) {
        lCurrentBytes = 128U * MemoryTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "MemoryTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "powertask")) {
        lCurrentBytes = 128U * PowerTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "PowerTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "sensorTask")) {
        lCurrentBytes = 128U * SensorTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "SensorTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "cpralgTask")) {
        lCurrentBytes = 128U * CprAlgTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "CprAlgTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "wirelessTask")) {
        lCurrentBytes = 128U * WirelessTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "WirelessTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "audioTask")) {
        lCurrentBytes = 128U * AudioTaskStackSize;
        lConfigQuantumBytes = 128U;
        lConfigName = "AudioTaskStackSize";
        lConfigUnitText = "x128B";
    } else if (systemDebugIsSameTaskName(taskName, "IDLE")) {
        lCurrentBytes = (uint32_t)configMINIMAL_STACK_SIZE * (uint32_t)sizeof(StackType_t);
        lConfigQuantumBytes = (uint32_t)sizeof(StackType_t);
        lConfigName = "configMINIMAL_STACK_SIZE";
        lConfigUnitText = "words";
    } else if (systemDebugIsSameTaskName(taskName, "Tmr Svc")) {
        lCurrentBytes = (uint32_t)configTIMER_TASK_STACK_DEPTH * (uint32_t)sizeof(StackType_t);
        lConfigQuantumBytes = (uint32_t)sizeof(StackType_t);
        lConfigName = "configTIMER_TASK_STACK_DEPTH";
        lConfigUnitText = "words";
    } else {
        return false;
    }

    *currentBytes = lCurrentBytes;
    *minBytes = usedBytes;
    *applyBytes = usedBytes + lConfigQuantumBytes;
    *configName = lConfigName;
    *configUnitText = lConfigUnitText;
    return true;
}

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle)
{
    UBaseType_t lIndex;

    if ((taskStatusArray == NULL) || (taskHandle == NULL)) {
        return NULL;
    }

    for (lIndex = 0U; lIndex < taskCount; lIndex++) {
        if (taskStatusArray[lIndex].xHandle == taskHandle) {
            return &taskStatusArray[lIndex];
        }
    }

    return NULL;
}

static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime)
{
    UBaseType_t lTaskCount;

    if ((taskStatusArray == NULL) || (taskCount == NULL) || (totalRunTime == NULL) || (capacity == 0U)) {
        return false;
    }

    lTaskCount = uxTaskGetNumberOfTasks();
    if ((lTaskCount == 0U) || (lTaskCount > capacity)) {
        return false;
    }

    *totalRunTime = 0U;
    lTaskCount = uxTaskGetSystemState(taskStatusArray, capacity, totalRunTime);
    if ((lTaskCount == 0U) || (*totalRunTime == 0U)) {
        return false;
    }

    *taskCount = lTaskCount;
    return true;
}

static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta)
{
    UBaseType_t lIndex;

    if ((currentTaskStats == NULL) || (previousTaskStats == NULL) || (totalRunTimeDelta == 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (logConsoleReply(transport,
        "sample %lu/%lu window=%lums",
        (unsigned long)(sampleIndex + 1U),
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lIndex = 0U; lIndex < currentTaskCount; lIndex++) {
        const TaskStatus_t *lPreviousTaskStatus;
        const char *lTaskName;
        uint32_t lTaskRunTimeDelta = 0U;
        uint32_t lUsagePercentX10;

        lPreviousTaskStatus = systemDebugFindTaskStatusByHandle(previousTaskStats,
            previousTaskCount,
            currentTaskStats[lIndex].xHandle);
        if ((lPreviousTaskStatus != NULL) &&
            (currentTaskStats[lIndex].ulRunTimeCounter >= lPreviousTaskStatus->ulRunTimeCounter)) {
            lTaskRunTimeDelta = currentTaskStats[lIndex].ulRunTimeCounter - lPreviousTaskStatus->ulRunTimeCounter;
        }

        lUsagePercentX10 = (uint32_t)((((uint64_t)lTaskRunTimeDelta * 1000ULL) +
            ((uint64_t)totalRunTimeDelta / 2ULL)) /
            (uint64_t)totalRunTimeDelta);
        lTaskName = (currentTaskStats[lIndex].pcTaskName != NULL) ? currentTaskStats[lIndex].pcTaskName : "unknown";
        if (logConsoleReply(transport,
            "%s: %lu.%lu%%",
            lTaskName,
            (unsigned long)(lUsagePercentX10 / 10U),
            (unsigned long)(lUsagePercentX10 % 10U)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static void systemDebugTaskUsageSampler(void *parameter)
{
    stSystemDebugTaskUsageContext *lContext = &gSystemDebugTaskUsageContext;
    TickType_t lLastWakeTime;
    uint32_t lSampleIndex;
    uint32_t lTransport = (uint32_t)parameter;

    (void)memset(lContext, 0, sizeof(*lContext));

    if (!systemDebugCaptureTaskUsageSnapshot(lContext->previousTaskStats,
        SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
        &lContext->previousTaskCount,
        &lContext->previousTotalRunTime)) {
        (void)logConsoleReply(lTransport, "ERROR: task runtime stats unavailable");
        gSystemDebugTaskUsageHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    lLastWakeTime = xTaskGetTickCount();
    for (lSampleIndex = 0U; lSampleIndex < SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT; lSampleIndex++) {
        vTaskDelayUntil(&lLastWakeTime, pdMS_TO_TICKS(SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS));

        if (!systemDebugCaptureTaskUsageSnapshot(lContext->currentTaskStats,
            SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
            &lContext->currentTaskCount,
            &lContext->currentTotalRunTime)) {
            (void)logConsoleReply(lTransport, "ERROR: task runtime stats unavailable");
            break;
        }

        if (lContext->currentTotalRunTime <= lContext->previousTotalRunTime) {
            (void)logConsoleReply(lTransport, "ERROR: invalid runtime stats window");
            break;
        }

        if (systemDebugReplyTaskUsageSample(lTransport,
            lSampleIndex,
            lContext->currentTaskStats,
            lContext->currentTaskCount,
            lContext->previousTaskStats,
            lContext->previousTaskCount,
            lContext->currentTotalRunTime - lContext->previousTotalRunTime) != CONSOLE_COMMAND_RESULT_OK) {
            break;
        }

        (void)memcpy(lContext->previousTaskStats,
            lContext->currentTaskStats,
            (size_t)lContext->currentTaskCount * sizeof(TaskStatus_t));
        lContext->previousTaskCount = lContext->currentTaskCount;
        lContext->previousTotalRunTime = lContext->currentTotalRunTime;
    }

    (void)logConsoleReply(lTransport, "top done");
    gSystemDebugTaskUsageHandle = NULL;
    vTaskDelete(NULL);
}

static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (gSystemDebugTaskUsageHandle != NULL) {
        if (logConsoleReply(transport, "ERROR: top sampler busy") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (xTaskCreate(systemDebugTaskUsageSampler,
        "TaskCpuMon",
        SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE,
        (void *)transport,
        SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY,
        &gSystemDebugTaskUsageHandle) != pdPASS) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (logConsoleReply(transport,
        "top started: %lums x %lu",
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleTaskStackHandler(uint32_t transport, int argc, char *argv[])
{
    TaskStatus_t lTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    UBaseType_t lTaskCount = 0U;
    uint32_t lTotalRunTime = 0U;
    UBaseType_t lIndex;

    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!systemDebugCaptureTaskUsageSnapshot(lTaskStats,
        SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
        &lTaskCount,
        &lTotalRunTime)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }
    (void)lTotalRunTime;

    if (logConsoleReply(transport,
        "stack watermark is historical minimum free stack since task creation; rerun after full workload") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lIndex = 0U; lIndex < lTaskCount; lIndex++) {
        const char *lTaskName = (lTaskStats[lIndex].pcTaskName != NULL) ? lTaskStats[lIndex].pcTaskName : "unknown";
        uint32_t lFreeWords = (uint32_t)lTaskStats[lIndex].usStackHighWaterMark;
        uint32_t lFreeBytes = lFreeWords * (uint32_t)sizeof(StackType_t);
        uint32_t lCurrentBytes = 0U;
        uint32_t lMinBytes = 0U;
        uint32_t lApplyBytes = 0U;
        const char *lConfigName = NULL;
        const char *lConfigUnitText = NULL;

        if (systemDebugResolveTaskStackConfig(lTaskName,
            0U,
            &lCurrentBytes,
            &lMinBytes,
            &lApplyBytes,
            &lConfigName,
            &lConfigUnitText)) {
            bool lUse128ByteConfigUnits;
            uint32_t lUsedBytes;
            uint32_t lCurrentConfigValue;
            uint32_t lMinConfigValue;
            uint32_t lApplyConfigValue;

            if (lCurrentBytes < lFreeBytes) {
                lUsedBytes = 0U;
            } else {
                lUsedBytes = lCurrentBytes - lFreeBytes;
            }

            (void)systemDebugResolveTaskStackConfig(lTaskName,
                lUsedBytes,
                &lCurrentBytes,
                &lMinBytes,
                &lApplyBytes,
                &lConfigName,
                &lConfigUnitText);

            lUse128ByteConfigUnits = strcmp(lConfigUnitText, "x128B") == 0;
            if (lUse128ByteConfigUnits) {
                lCurrentConfigValue = systemDebugRoundUpDivide(lCurrentBytes, 128U);
                lMinConfigValue = systemDebugRoundUpDivide(lMinBytes, 128U);
                lApplyConfigValue = systemDebugRoundUpDivide(lApplyBytes, 128U);
            } else {
                lCurrentConfigValue = systemDebugBytesToWords(lCurrentBytes);
                lMinConfigValue = systemDebugBytesToWords(lMinBytes);
                lApplyConfigValue = systemDebugBytesToWords(lApplyBytes);
            }

            if (logConsoleReply(transport,
                "%s: cur=%luB used_peak=%luB free_min=%luB cfg=%s cur=%lu%s min=%lu%s apply=%lu%s",
                lTaskName,
                (unsigned long)lCurrentBytes,
                (unsigned long)lUsedBytes,
                (unsigned long)lFreeBytes,
                lConfigName,
                (unsigned long)lCurrentConfigValue,
                lConfigUnitText,
                (unsigned long)lMinConfigValue,
                lConfigUnitText,
                (unsigned long)lApplyConfigValue,
                lConfigUnitText) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }
        } else {
            if (logConsoleReply(transport,
                "%s: free_min=%luB cfg=unknown",
                lTaskName,
                (unsigned long)lFreeBytes) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }
        }
    }

    if (logConsoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}
#endif

/**
* @brief : Reply with firmware and hardware version strings.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (logConsoleReply(transport,
        "Firmware: %s\nVersion: %s\nHardware: %s\nOK",
        systemGetFirmwareName(),
        systemGetFirmwareVersion(),
        systemGetHardwareVersion()) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

/**
* @brief : Reply with current system runtime status.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (logConsoleReply(transport,
        "Mode: %s\nOK",
        systemGetModeString(systemGetMode())) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (logConsoleReply(transport, "rebooting...") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    vTaskDelay(pdMS_TO_TICKS(10U));
#else
    HAL_Delay(10U);
#endif

    __DSB();
    NVIC_SystemReset();

    return CONSOLE_COMMAND_RESULT_OK;
}

static const char *systemDebugWirelessStateText(eWirelessState state)
{
    switch (state) {
        case eWIRELESS_STATE_INIT:
            return "init";
        case eWIRELESS_STATE_NORMAL:
            return "normal";
        case eWIRELESS_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static const char *systemDebugWirelessWifiStateText(eWirelessWifiState state)
{
    switch (state) {
        case WIRELESS_WIFI_IDLE:
            return "idle";
        case WIRELESS_WIFI_INITIALIZING:
            return "initializing";
        case WIRELESS_WIFI_READY:
            return "ready";
        case WIRELESS_WIFI_WAITING_CONNECTION:
            return "waiting";
        case WIRELESS_WIFI_CONNECTED:
            return "connected";
        case WIRELESS_WIFI_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static const char *systemDebugWirelessIotStateText(eWirelessIotState state)
{
    switch (state) {
        case WIRELESS_IOT_IDLE:
            return "idle";
        case WIRELESS_IOT_WAIT_WIFI:
            return "wait_wifi";
        case WIRELESS_IOT_WAIT_AUTH:
            return "wait_auth";
        case WIRELESS_IOT_AUTH_READY:
            return "auth_ready";
        case WIRELESS_IOT_MQTT_CONNECTING:
            return "mqtt_connecting";
        case WIRELESS_IOT_MQTT_READY:
            return "mqtt_ready";
        case WIRELESS_IOT_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static eConsoleCommandResult systemDebugWirelessReplyStatus(uint32_t transport)
{
    const eWirelessState *lState = wirelessGetStatus();
    eWirelessWifiState lWifiState = wirelessGetWifiState();
    eWirelessIotState lIotState = wirelessGetIotState();
    uint16_t lBleRxLen = wirelessGetBleRxLength();
    uint16_t lWifiRxLen = wirelessGetWifiRxLength();

    if (lState == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (logConsoleReply(transport,
        "state=%s wifi=%s iot=%s ble_rx=%u wifi_rx=%u\nOK",
        systemDebugWirelessStateText(*lState),
        systemDebugWirelessWifiStateText(lWifiState),
        systemDebugWirelessIotStateText(lIotState),
        (unsigned int)lBleRxLen,
        (unsigned int)lWifiRxLen) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleWirelessHandler(uint32_t transport, int argc, char *argv[])
{
    if ((argc < 2) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
        return systemDebugWirelessReplyStatus(transport);
    }
    if (strcmp(argv[1], "ble_on") == 0) {
        if ((argc != 2) || !wirelessSetBleEnabled(true)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "ble_off") == 0) {
        if ((argc != 2) || !wirelessSetBleEnabled(false)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "wifi_on") == 0) {
        if ((argc != 2) || !wirelessSetWifiEnabled(true)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "wifi_off") == 0) {
        if ((argc != 2) || !wirelessSetWifiEnabled(false)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "wifi_connect") == 0) {
        if ((argc != 4) || !wirelessSetWifiCredentials((const uint8_t *)argv[2],
                                                       (uint8_t)strlen(argv[2]),
                                                       (const uint8_t *)argv[3],
                                                       (uint8_t)strlen(argv[3]))) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "mqtt_on") == 0) {
        if ((argc != 2) || !wirelessSetMqttEnabled(true)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else if (strcmp(argv[1], "mqtt_off") == 0) {
        if ((argc != 2) || !wirelessSetMqttEnabled(false)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    } else {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (logConsoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleSensorHandler(uint32_t transport, int argc, char *argv[])
{
    stLis2hh12Sample lAccSample;
    uint16_t lForceRaw = 0U;
    eDrvStatus lAccStatus;
    eDrvStatus lForceStatus;

    (void)argv;

    if ((argc == 2) && (strcmp(argv[1], "help") == 0)) {
        if (consoleReply(transport,
            "sensor\n"
            "  read accelerometer x/y/z and force ADC raw value once\n"
            "sensor init\n"
            "  retry sensor initialization\n"
            "sensor status\n"
            "  show sensor initialization status\n"
            "sensor help\n"
            "  show this help\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((argc == 2) && (strcmp(argv[1], "init") == 0)) {
        if (consoleReply(transport, "sensor init %s", sensorInit() ? "ok" : "fail") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport, "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((argc == 2) && (strcmp(argv[1], "status") == 0)) {
        stSensorInitStatus lStatus;

        sensorGetInitStatus(&lStatus);
        if (consoleReply(transport,
            "sensor init: attempts=%lu init=%d queue_ready=%d force_ready=%d acc_ready=%d",
            (unsigned long)lStatus.attemptCount,
            (int)lStatus.initialized,
            (int)lStatus.queueReady,
            (int)lStatus.forceReady,
            (int)lStatus.accReady) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport,
            "sensor status: queue=%ld force=%d acc_id=%d who=0x%02X acc_init=%d drops=%lu",
            (long)lStatus.queueStatus,
            (int)lStatus.forceStatus,
            (int)lStatus.accReadIdStatus,
            (unsigned int)lStatus.accWhoAmI,
            (int)lStatus.accInitStatus,
            (unsigned long)sensorGetDropCount()) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport, "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lAccStatus = lis2hh12ReadRaw(LIS2HH12_DEV0, &lAccSample);
    lForceStatus = drvAdcReadRaw(DRVADC_FORCE, &lForceRaw);

    if (consoleReply(transport,
        "acc: status=%d x=%d y=%d z=%d",
        (int)lAccStatus,
        (int)lAccSample.x,
        (int)lAccSample.y,
        (int)lAccSample.z) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "force: status=%d raw=%u",
        (int)lForceStatus,
        (unsigned int)lForceRaw) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsolePowerDebugHandler(uint32_t transport, int argc, char *argv[])
{
    const PowerManager *lPowerManager;

    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lPowerManager = powerGetManager();
    if (lPowerManager == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "power real voltage(10mV): bat=%u dc=%u 5v0=%u 3v3=%u bat_level=%u charge_state=%u chg=%u done=%u",
        (unsigned int)lPowerManager->voltage.batteryMv,
        (unsigned int)lPowerManager->voltage.dcMv,
        (unsigned int)lPowerManager->voltage.v5v0Mv,
        (unsigned int)lPowerManager->voltage.v3v3Mv,
        (unsigned int)lPowerManager->BatLevel,
        (unsigned int)lPowerManager->chargeState,
        (unsigned int)lPowerManager->isChargingStatusHigh,
        (unsigned int)lPowerManager->isChargeDoneStatusHigh) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

bool systemDebugBackgroundServicesInit(void)
{
    if (gSystemDebugBackgroundServicesReady) {
        return true;
    }

    if (!logInit()) {
        return false;
    }

    if (!systemDebugConsoleRegister()) {
        return false;
    }

    if (!drvAnlogIicDebugConsoleRegister()) {
        return false;
    }

    if (!drvAdcDebugConsoleRegister()) {
        return false;
    }

    if (!drvGpioDebugConsoleRegister()) {
        return false;
    }

    if (!drvIicDebugConsoleRegister()) {
        return false;
    }

    if (!drvSpiDebugConsoleRegister()) {
        return false;
    }

    if (!drvUartDebugConsoleRegister()) {
        return false;
    }

    gSystemDebugBackgroundServicesReady = true;
    LOG_I(SYSTEM_DEBUG_LOG_TAG, "background console ready");
    return true;
}

void systemDebugBackgroundServicesProcess(void)
{
    if (!gSystemDebugBackgroundServicesReady) {
        return;
    }

    ConsoleBackGournd();
}

bool systemDebugConsoleRegister(void)
{
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (!logRegisterConsole(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemRebootConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemWirelessConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemSensorConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemPowerDebugConsoleCommand)) {
        return false;
    }

    if (!memoryDebugConsoleRegister()) {
        return false;
    }

    if (!protcolMgrDebugConsoleRegister()) {
        return false;
    }

    if (!logRegisterConsole(&gSystemTaskUsageConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemTaskStackConsoleCommand)) {
        return false;
    }

    return true;
#elif (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1)
    if (!logRegisterConsole(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemRebootConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemWirelessConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemSensorConsoleCommand)) {
        return false;
    }

    if (!logRegisterConsole(&gSystemPowerDebugConsoleCommand)) {
        return false;
    }

    if (!memoryDebugConsoleRegister()) {
        return false;
    }

    if (!protcolMgrDebugConsoleRegister()) {
        return false;
    }

    return true;
#else
    return true;
#endif
}
/**************************End of file********************************/
