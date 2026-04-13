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

#include "rtos.h"

#include "sysmgr.h"
#include "system.h"
#include "system_debug.h"
#include "../../rep/driver/drvadc/drvadc_debug.h"
#include "../../rep/driver/drvanlogiic/drvanlogiic_debug.h"
#include "../../rep/driver/drviic/drviic_debug.h"
#include "../../rep/service/console/console.h"
#include "../../rep/service/console/log.h"
#include "../manager/power/power.h"
#include "../manager/wireless/wireless.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"

#define SYSTASK_LOG_TAG "systask"
#define SYSTASK_STACK_DEPTH_FROM_BYTES(bytes) ((uint32_t)(bytes) / (uint32_t)sizeof(uint32_t))

static void systemCommTaskEntry(void *argument);
static void systemMemoryTaskEntry(void *argument);
static void systemPowerTaskEntry(void *argument);
static void systemWirelessTaskEntry(void *argument);
static void systemAudioTaskEntry(void *argument);
static void systemBackgroundTaskEntry(void *argument);

static bool gSystaskWorkerTasksCreated = false;
static bool gSystaskBackgroundServicesReady = false;

static repRtosTaskHandle gSystemCommTaskHandle = NULL;
static repRtosTaskHandle gSystemMemoryTaskHandle = NULL;
static repRtosTaskHandle gSystemPowerTaskHandle = NULL;
static repRtosTaskHandle gSystemWirelessTaskHandle = NULL;
static repRtosTaskHandle gSystemAudioTaskHandle = NULL;
static repRtosTaskHandle gSystemBackgroundTaskHandle = NULL;

static const stRepRtosTaskConfig gSystemCommTaskConfig = {
	.name = "commTask",
	.entry = systemCommTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * CommTaskStackSize),
	.priority = CommTaskPriority,
	.handle = &gSystemCommTaskHandle,
};

static const stRepRtosTaskConfig gSystemMemoryTaskConfig = {
	.name = "memorytask",
	.entry = systemMemoryTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * MemoryTaskStackSize),
	.priority = MemoryTaskPriority,
	.handle = &gSystemMemoryTaskHandle,
};

static const stRepRtosTaskConfig gSystemPowerTaskConfig = {
	.name = "powertask",
	.entry = systemPowerTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * PowerTaskStackSize),
	.priority = PowerTaskPriority,
	.handle = &gSystemPowerTaskHandle,
};

static const stRepRtosTaskConfig gSystemWirelessTaskConfig = {
	.name = "wirelessTask",
	.entry = systemWirelessTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * WirelessTaskStackSize),
	.priority = WirelessTaskPriority,
	.handle = &gSystemWirelessTaskHandle,
};

static const stRepRtosTaskConfig gSystemAudioTaskConfig = {
	.name = "audioTask",
	.entry = systemAudioTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * AudioTaskStackSize),
	.priority = AudioTaskPriority,
	.handle = &gSystemAudioTaskHandle,
};

static const stRepRtosTaskConfig gSystemBackgroundTaskConfig = {
	.name = "backgroundTask",
	.entry = systemBackgroundTaskEntry,
	.argument = NULL,
	.stackSize = SYSTASK_STACK_DEPTH_FROM_BYTES(128U * BackgroundTaskStackSize),
	.priority = BackgroundTaskPriority,
	.handle = &gSystemBackgroundTaskHandle,
};

static void systaskCommProcess(void)
{
}

static void systaskMemoryProcess(void)
{
}

static void systaskAudioProcess(void)
{
}

static bool systaskInitBackgroundServices(void)
{
	if (gSystaskBackgroundServicesReady) {
		return true;
	}

	if (!logInit()) {
		return false;
	}

	if (!consoleInit()) {
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

	if (!drvIicDebugConsoleRegister()) {
		return false;
	}

	gSystaskBackgroundServicesReady = true;
	LOG_I(SYSTASK_LOG_TAG, "background console ready");
	return true;
}


static void systemCommTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systaskCommProcess();
		(void)repRtosDelayMs(CommTaskInterval);
	}
}

static void systemMemoryTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systaskMemoryProcess();
		(void)repRtosDelayMs(MemoryTaskInterval);
	}
}

static void systemPowerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		powerProcess();
		(void)repRtosDelayMs(PowerTaskInterval);
	}
}

static void systemWirelessTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		wirelessProcess();
		(void)repRtosDelayMs(WirelessTaskInterval);
	}
}

static void systemAudioTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systaskAudioProcess();
		(void)repRtosDelayMs(AudioTaskInterval);
	}
}

static void systemBackgroundTaskEntry(void *argument)
{
	systaskRunBackgroundTask(argument);
}

bool systaskCreateBackgroundTask(void)
{
	if (gSystemBackgroundTaskHandle != NULL) {
		return true;
	}

	return repRtosTaskCreate(&gSystemBackgroundTaskConfig) == REP_RTOS_STATUS_OK;
}

bool systaskCreateWorkerTasks(void)
{
	if (gSystaskWorkerTasksCreated) {
		return true;
	}

	(void)repRtosTaskCreate(&gSystemCommTaskConfig);
	(void)repRtosTaskCreate(&gSystemMemoryTaskConfig);
	(void)repRtosTaskCreate(&gSystemPowerTaskConfig);
	(void)repRtosTaskCreate(&gSystemWirelessTaskConfig);
	(void)repRtosTaskCreate(&gSystemAudioTaskConfig);
	if (!systaskCreateBackgroundTask()) {
		return false;
	}

	if ((gSystemCommTaskHandle == NULL) ||
		(gSystemMemoryTaskHandle == NULL) ||
		(gSystemPowerTaskHandle == NULL) ||
		(gSystemWirelessTaskHandle == NULL) ||
		(gSystemAudioTaskHandle == NULL) ||
		(gSystemBackgroundTaskHandle == NULL)) {
		return false;
	}

	gSystaskWorkerTasksCreated = true;
	return true;
}

void systaskRunSystemTask(void *argument)
{
	(void)argument;
	for (;;) {
		systemManagerRun();
		(void)repRtosDelayMs(SystemTaskInterval);
	}
}

void systaskRunBackgroundTask(void *argument)
{
	(void)argument;

	for (;;) {
		if (systaskInitBackgroundServices()) {
			consoleProcess();
		}
		
		(void)repRtosDelayMs(BackgroundTaskInterval);
	}
}

/**************************End of file********************************/
