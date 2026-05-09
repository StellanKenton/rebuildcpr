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
#include "../manager/cpralg/cpralgmgr.h"
#include "../manager/sensor/sensor.h"
#include "../manager/wireless/wireless.h"
#include "../manager/audio/audio.h"
#include "../manager/selfcheck/selfcheck_fault.h"

#define SYSTASK_LOG_TAG "systask"
#define SYSTASK_STACK_DEPTH_FROM_BYTES(bytes) ((uint32_t)(bytes) / (uint32_t)sizeof(uint32_t))

volatile uint32_t gSystemFaultTraceStage = 0U;

static bool gSystaskWorkerTasksCreated = false;

static repRtosTaskHandle gSystemMemoryTaskHandle = NULL;
static repRtosTaskHandle gSystemPowerTaskHandle = NULL;
static repRtosTaskHandle gSystemSensorTaskHandle = NULL;
static repRtosTaskHandle gSystemCprAlgTaskHandle = NULL;
static repRtosTaskHandle gSystemWirelessTaskHandle = NULL;
static repRtosTaskHandle gSystemAudioTaskHandle = NULL;

static void memoryTaskEntry(void *argument);
static void powerTaskEntry(void *argument);
static void sensorTaskEntry(void *argument);
static void cprAlgTaskEntry(void *argument);
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

static const stRepRtosTaskConfig gSystemSensorTaskConfig = {
	.name = "sensorTask",
	.entry = sensorTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * SensorTaskStackSize),
	.priority = SensorTaskPriority,
	.handle = &gSystemSensorTaskHandle,
};

static const stRepRtosTaskConfig gSystemCprAlgTaskConfig = {
	.name = "cpralgTask",
	.entry = cprAlgTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * CprAlgTaskStackSize),
	.priority = CprAlgTaskPriority,
	.handle = &gSystemCprAlgTaskHandle,
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
		if (!memoryIsReady()) {
			(void)memoryInit();
		} else {
			memoryProcess();
		}
		(void)repRtosDelayMs(MemoryTaskInterval);
	}
}

static void powerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		if (!powerIsReady()) {
			(void)powerInit();
		} else {
			powerProcess();
		}
		(void)repRtosDelayMs(PowerTaskInterval);
	}
}

static void sensorTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		if (!sensorIsReady()) {
			(void)sensorInit();
		} else {
			sensorProcess();
		}
		(void)repRtosDelayMs(SensorTaskInterval);
	}
}

static void cprAlgTaskEntry(void *argument)
{
	(void)argument;
	(void)cprAlgMgrInit();

	for (;;) {
		cprAlgMgrProcess();
		(void)repRtosDelayMs(CprAlgTaskInterval);
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
		audioProcess();
		(void)repRtosDelayMs(AudioTaskInterval);
	}
}

bool systaskCreateWorkerTasks(void)
{
	static bool lCreateFailedLogged = false;
	eRepRtosStatus memoryStatus;
	eRepRtosStatus powerStatus;
	eRepRtosStatus sensorStatus;
	eRepRtosStatus cprAlgStatus;
	eRepRtosStatus wirelessStatus;
	eRepRtosStatus audioStatus;

	if (gSystaskWorkerTasksCreated) {
		return true;
	}

	memoryStatus = repRtosTaskCreate(&gSystemMemoryTaskConfig);
	powerStatus = repRtosTaskCreate(&gSystemPowerTaskConfig);
	sensorStatus = repRtosTaskCreate(&gSystemSensorTaskConfig);
	cprAlgStatus = repRtosTaskCreate(&gSystemCprAlgTaskConfig);
	wirelessStatus = repRtosTaskCreate(&gSystemWirelessTaskConfig);
	audioStatus = repRtosTaskCreate(&gSystemAudioTaskConfig);

	if ((gSystemMemoryTaskHandle == NULL) ||
		(gSystemPowerTaskHandle == NULL) ||
		(gSystemSensorTaskHandle == NULL) ||
		(gSystemCprAlgTaskHandle == NULL) ||
		(gSystemWirelessTaskHandle == NULL) ||
		(gSystemAudioTaskHandle == NULL)) {
		if (!lCreateFailedLogged) {
			lCreateFailedLogged = true;
			LOG_E(SYSTASK_LOG_TAG, "worker create fail mem=%d power=%d sensor=%d cpralg=%d wireless=%d audio=%d handles=%p/%p/%p/%p/%p/%p",
				  (int)memoryStatus,
				  (int)powerStatus,
				  (int)sensorStatus,
				  (int)cprAlgStatus,
				  (int)wirelessStatus,
				  (int)audioStatus,
				  gSystemMemoryTaskHandle,
				  gSystemPowerTaskHandle,
				  gSystemSensorTaskHandle,
				  gSystemCprAlgTaskHandle,
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
		if ((systemGetMode() == eSYSTEM_STANDBY_MODE) || (systemGetMode() == eSYSTEM_NORMAL_MODE)) {
			drvAdcBackground();         // ADC background processing
			selfCheckFaultProcess100ms();
		}
		powerLedProcess();          // Power LED state update
		cprAlgMgrDisplayProcess();
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
