/***********************************************************************************
* @file     : systask.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/

#include "systask.h"

#include "../../Core/Inc/iwdg.h"
#include "rtos.h"

#include "sysmgr.h"
#include "system_debug.h"
#include "../../rep/service/log/log.h"
#include "../../rep/driver/drvadc/drvadc.h"
#include "../manager/memory/memory.h"
#include "../manager/power/power.h"
#include "../manager/wireless/wireless.h"

#define SYSTASK_LOG_TAG "systask"
#define SYSTASK_STACK_DEPTH_FROM_BYTES(bytes) ((uint32_t)(bytes) / (uint32_t)sizeof(uint32_t))

volatile uint32_t gSystemFaultTraceStage = 0U;

static bool gSystaskWorkerTasksCreated = false;

static repRtosTaskHandle gSystemMemoryTaskHandle = NULL;
static repRtosTaskHandle gSystemPowerTaskHandle = NULL;
static repRtosTaskHandle gSystemWirelessTaskHandle = NULL;
static repRtosTaskHandle gSystemAudioTaskHandle = NULL;

static void memoryTaskEntry(void *argument);
static void powerTaskEntry(void *argument);
static void wirelessTaskEntry(void *argument);
static void audioTaskEntry(void *argument);

static const stRepRtosTaskConfig gSystemMemoryTaskConfig = {
	.name = "memorytask",
	.entry = memoryTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * MemoryTaskStackSize),
	.priority = MemoryTaskPriority,
	.handle = &gSystemMemoryTaskHandle,
};

static const stRepRtosTaskConfig gSystemPowerTaskConfig = {
	.name = "powertask",
	.entry = powerTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * PowerTaskStackSize),
	.priority = PowerTaskPriority,
	.handle = &gSystemPowerTaskHandle,
};

static const stRepRtosTaskConfig gSystemWirelessTaskConfig = {
	.name = "wirelessTask",
	.entry = wirelessTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * WirelessTaskStackSize),
	.priority = WirelessTaskPriority,
	.handle = &gSystemWirelessTaskHandle,
};

static const stRepRtosTaskConfig gSystemAudioTaskConfig = {
	.name = "audioTask",
	.entry = audioTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * AudioTaskStackSize),
	.priority = AudioTaskPriority,
	.handle = &gSystemAudioTaskHandle,
};

static void memoryTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		memoryProcess();
		(void)repRtosDelayMs(MemoryTaskInterval);
	}
}

static void powerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(PowerTaskInterval);
	}
}

static void wirelessTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		wirelessProcess();
		(void)repRtosDelayMs(WirelessTaskInterval);
	}
}

static void audioTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		
		(void)repRtosDelayMs(AudioTaskInterval);
	}
}

bool systaskCreateWorkerTasks(void)
{
	static bool lCreateFailedLogged = false;
	eRepRtosStatus memoryStatus;
	eRepRtosStatus powerStatus;
	eRepRtosStatus wirelessStatus;
	eRepRtosStatus audioStatus;

	if (gSystaskWorkerTasksCreated) {
		return true;
	}

	memoryStatus = repRtosTaskCreate(&gSystemMemoryTaskConfig);
	powerStatus = repRtosTaskCreate(&gSystemPowerTaskConfig);
	wirelessStatus = repRtosTaskCreate(&gSystemWirelessTaskConfig);
	audioStatus = repRtosTaskCreate(&gSystemAudioTaskConfig);

	if ((gSystemMemoryTaskHandle == NULL) ||
		(gSystemPowerTaskHandle == NULL) ||
		(gSystemWirelessTaskHandle == NULL) ||
		(gSystemAudioTaskHandle == NULL)) {
		if (!lCreateFailedLogged) {
			lCreateFailedLogged = true;
			LOG_E(SYSTASK_LOG_TAG, "worker create fail mem=%d power=%d wireless=%d audio=%d handles=%p/%p/%p/%p",
				  (int)memoryStatus,
				  (int)powerStatus,
				  (int)wirelessStatus,
				  (int)audioStatus,
				  gSystemMemoryTaskHandle,
				  gSystemPowerTaskHandle,
				  gSystemWirelessTaskHandle,
				  gSystemAudioTaskHandle);
		}
		return false;
	}

	gSystaskWorkerTasksCreated = true;
	LOG_I(SYSTASK_LOG_TAG, "worker tasks ready");
	return true;
}

void systemTaskEntry(void *argument)
{
	(void)argument;
	for (;;) {
		systemManagerRun();         // System Manager
        /*************** Background Services **********************/
        drvAdcBackground();         // ADC background processing
        powerProcess();             // Power sampling and battery level update
		powerLedProcess();          // Power LED state update
		if (systemDebugBackgroundServicesInit()) {
			systemDebugBackgroundServicesProcess();
		}
        /******************   Watch Dog  *************************/
        (void)HAL_IWDG_Refresh(&hiwdg);
        /*********************************************************/
		(void)repRtosDelayMs(SystemTaskInterval);
	}
}

/**************************End of file********************************/
