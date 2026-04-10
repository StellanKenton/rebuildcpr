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

#include "system.h"

#include "../manager/manager.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"

static bool gSystaskWorkerTasksCreated = false;
static uint16_t gSystaskBackgroundCounter = 0U;

static osThreadId_t gSystemCommTaskHandle = NULL;
static osThreadId_t gSystemMemoryTaskHandle = NULL;
static osThreadId_t gSystemPowerTaskHandle = NULL;
static osThreadId_t gSystemWirelessTaskHandle = NULL;
static osThreadId_t gSystemAudioTaskHandle = NULL;
static osThreadId_t gSystemBackgroundTaskHandle = NULL;

static const osThreadAttr_t gSystemCommTaskAttributes = {
	.name = "commTask",
	.stack_size = 128U * 4U,
	.priority = (osPriority_t)osPriorityBelowNormal7,
};

static const osThreadAttr_t gSystemMemoryTaskAttributes = {
	.name = "memorytask",
	.stack_size = 256U * 4U,
	.priority = (osPriority_t)osPriorityLow5,
};

static const osThreadAttr_t gSystemPowerTaskAttributes = {
	.name = "powertask",
	.stack_size = 64U * 4U,
	.priority = (osPriority_t)osPriorityBelowNormal,
};

static const osThreadAttr_t gSystemWirelessTaskAttributes = {
	.name = "wirelessTask",
	.stack_size = 512U * 4U,
	.priority = (osPriority_t)osPriorityBelowNormal7,
};

static const osThreadAttr_t gSystemAudioTaskAttributes = {
	.name = "audioTask",
	.stack_size = 256U * 4U,
	.priority = (osPriority_t)osPriorityLow,
};

static const osThreadAttr_t gSystemBackgroundTaskAttributes = {
	.name = "backgroundTask",
	.stack_size = 512U * 4U,
	.priority = (osPriority_t)osPriorityLow,
};

static void systemCommTaskStep(void)
{
}

static void systemMemoryTaskStep(void)
{
	if (systemGetMode() != eSYSTEM_UPDATE_MODE) {
		return;
	}

	if (!managerUpdateStart()) {
		systemSetMode(eSYSTEM_DIAGNOSTIC_MODE);
		systemSyncIndicators();
		return;
	}

	managerUpdateProcess();
}

static void systemPowerTaskStep(void)
{
	if (systemGetMode() == eSYSTEM_NORMAL_MODE) {
		managerPowerProcess();
	}
}

static void systemWirelessTaskStep(void)
{
}

static void systemAudioTaskStep(void)
{
}

static void systemBackgroundTaskStep(void)
{
	if (systemGetMode() == eSYSTEM_NORMAL_MODE) {
		gSystaskBackgroundCounter = (uint16_t)((gSystaskBackgroundCounter + 1U) % 1000U);
		(void)tm1651PortShowNumber3(gSystaskBackgroundCounter);
		(void)pca9535PortLedLightNum((uint8_t)((gSystaskBackgroundCounter % 8U) + 1U));
	}
}

static void systemCommTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemCommTaskStep();
		osDelay(20U);
	}
}

static void systemMemoryTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemMemoryTaskStep();
		osDelay(100U);
	}
}

static void systemPowerTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemPowerTaskStep();
		osDelay(100U);
	}
}

static void systemWirelessTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemWirelessTaskStep();
		osDelay(50U);
	}
}

static void systemAudioTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemAudioTaskStep();
		osDelay(20U);
	}
}

static void systemBackgroundTaskEntry(void *argument)
{
	(void)argument;

	for (;;) {
		systemBackgroundTaskStep();
		osDelay(250U);
	}
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
	gSystemBackgroundTaskHandle = osThreadNew(systemBackgroundTaskEntry, NULL, &gSystemBackgroundTaskAttributes);

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

	systemBootstrap();
	for (;;) {
		systemDefaultTaskStep();
		osDelay(50U);
	}
}

/**************************End of file********************************/
