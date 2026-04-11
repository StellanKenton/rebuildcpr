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

#include "cmsis_os.h"

#include "sysmgr.h"
#include "system.h"
#include "../../rep/service/console/console.h"
#include "../../rep/service/console/log.h"
#include "../manager/manager.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"

#define SYSTASK_LOG_TAG "systask"

static bool gSystaskWorkerTasksCreated = false;
static bool gSystaskBackgroundServicesReady = false;

static osThreadId_t gSystemCommTaskHandle = NULL;
static osThreadId_t gSystemMemoryTaskHandle = NULL;
static osThreadId_t gSystemPowerTaskHandle = NULL;
static osThreadId_t gSystemWirelessTaskHandle = NULL;
static osThreadId_t gSystemAudioTaskHandle = NULL;
static osThreadId_t gSystemBackgroundTaskHandle = NULL;

static const osThreadAttr_t gSystemCommTaskAttributes = {
	.name = "commTask",
	.stack_size = 128U * CommTaskStackSize,
	.priority = (osPriority_t)CommTaskPriority,
};

static const osThreadAttr_t gSystemMemoryTaskAttributes = {
	.name = "memorytask",
	.stack_size = 256U * MemoryTaskStackSize,
	.priority = (osPriority_t)MemoryTaskPriority,
};

static const osThreadAttr_t gSystemPowerTaskAttributes = {
	.name = "powertask",
	.stack_size = 64U * PowerTaskStackSize,
	.priority = (osPriority_t)PowerTaskPriority,
};

static const osThreadAttr_t gSystemWirelessTaskAttributes = {
	.name = "wirelessTask",
	.stack_size = 512U * WirelessTaskStackSize,
	.priority = (osPriority_t)WirelessTaskPriority,
};

static const osThreadAttr_t gSystemAudioTaskAttributes = {
	.name = "audioTask",
	.stack_size = 256U * AudioTaskStackSize,
	.priority = (osPriority_t)AudioTaskPriority,
};

static const osThreadAttr_t gSystemBackgroundTaskAttributes = {
	.name = "backgroundTask",
	.stack_size = 512U * BackgroundTaskStackSize,
	.priority = (osPriority_t)BackgroundTaskPriority,
};

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

	gSystaskBackgroundServicesReady = true;
	LOG_I(SYSTASK_LOG_TAG, "background console ready");
	return true;
}


static void systemCommTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		commTaskManager();
		osDelay(CommTaskInterval);
	}
}

static void systemMemoryTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		memoryTaskManager();
		osDelay(MemoryTaskInterval);
	}
}

static void systemPowerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		powerTaskManager();
		osDelay(PowerTaskInterval);
	}
}

static void systemWirelessTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		wirelessTaskManager();
		osDelay(WirelessTaskInterval);
	}
}

static void systemAudioTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		audioTaskManager();
		osDelay(AudioTaskInterval);
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

	gSystemBackgroundTaskHandle = osThreadNew(systemBackgroundTaskEntry, NULL, &gSystemBackgroundTaskAttributes);
	return gSystemBackgroundTaskHandle != NULL;
}

bool systaskCreateWorkerTasks(void)
{
	if (gSystaskWorkerTasksCreated) {
		return true;
	}

	gSystemCommTaskHandle = osThreadNew(systemCommTaskEntry, NULL, &gSystemCommTaskAttributes);
	gSystemMemoryTaskHandle = osThreadNew(systemMemoryTaskEntry, NULL, &gSystemMemoryTaskAttributes);
	gSystemPowerTaskHandle = osThreadNew(systemPowerTaskEntry, NULL, &gSystemPowerTaskAttributes);
	gSystemWirelessTaskHandle = osThreadNew(systemWirelessTaskEntry, NULL, &gSystemWirelessTaskAttributes);
	gSystemAudioTaskHandle = osThreadNew(systemAudioTaskEntry, NULL, &gSystemAudioTaskAttributes);
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
	(void)systaskCreateWorkerTasks();

	for (;;) {
		systemManagerRun();
		osDelay(SystemTaskInterval);
	}
}

void systaskRunBackgroundTask(void *argument)
{
	(void)argument;

	for (;;) {
		if (systaskInitBackgroundServices()) {
			consoleProcess();
		}
		
		osDelay(BackgroundTaskInterval);
	}
}

/**************************End of file********************************/
