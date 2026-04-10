/***********************************************************************************
* @file     : system.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system.h"

#include <stdint.h>

#include "cmsis_os.h"

#include "../manager/manager.h"
#include "../port/drvadc_port.h"
#include "../port/drvgpio_port.h"
#include "../port/drviic_port.h"
#include "../port/drvspi_port.h"
#include "../port/drvuart_port.h"
#include "../port/drvusb_port.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"
#include "../../rep/driver/drvadc/drvadc.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "../../rep/driver/drviic/drviic.h"
#include "../../rep/driver/drvspi/drvspi.h"
#include "../../rep/driver/drvuart/drvuart.h"
#include "../../rep/driver/drvusb/drvusb.h"
#include "../../rep/module/lsm6/lsm6.h"
#include "../../rep/module/w25qxxx/w25qxxx.h"

typedef enum eSystemInitStage {
    SYSTEM_INIT_STAGE_GPIO = 0,
    SYSTEM_INIT_STAGE_ADC,
    SYSTEM_INIT_STAGE_IIC,
    SYSTEM_INIT_STAGE_SPI,
    SYSTEM_INIT_STAGE_UART,
    SYSTEM_INIT_STAGE_USB,
    SYSTEM_INIT_STAGE_MODULE,
    SYSTEM_INIT_STAGE_MANAGER,
    SYSTEM_INIT_STAGE_COMPLETE,
    SYSTEM_INIT_STAGE_FAULT,
} eSystemInitStage;

typedef struct stSystemContext {
    bool isBootstrapped;
    bool indicatorsDirty;
    bool workerTasksCreated;
    uint16_t backgroundCounter;
    eSystemInitStage initStage;
} stSystemContext;

static stSystemContext gSystemContext = {
    .isBootstrapped = false,
    .indicatorsDirty = false,
    .workerTasksCreated = false,
    .backgroundCounter = 0U,
    .initStage = SYSTEM_INIT_STAGE_GPIO,
};

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

static void systemCommTaskEntry(void *argument);
static void systemMemoryTaskEntry(void *argument);
static void systemPowerTaskEntry(void *argument);
static void systemWirelessTaskEntry(void *argument);
static void systemAudioTaskEntry(void *argument);
static void systemBackgroundTaskEntry(void *argument);

static void systemApplyIndicators(eSystemMode mode)
{
    switch (mode) {
        case eSYSTEM_INIT_MODE:
            (void)pca9535PortLedPowerShow(false, false, false);
            (void)pca9535PortLedPressShow(false, false, true);
            (void)tm1651PortShowNone();
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            (void)pca9535PortLedPowerShow(false, false, true);
            (void)pca9535PortLedPressShow(false, false, false);
            (void)tm1651PortShowNone();
            break;
        case eSYSTEM_STANDBY_MODE:
            (void)pca9535PortLedPowerShow(false, true, false);
            (void)pca9535PortLedPressShow(false, false, false);
            (void)tm1651PortShowNumber3(0U);
            break;
        case eSYSTEM_NORMAL_MODE:
            (void)pca9535PortLedPowerShow(false, true, false);
            (void)pca9535PortLedPressShow(false, false, false);
            break;
        case eSYSTEM_UPDATE_MODE:
            (void)pca9535PortLedPowerShow(false, false, true);
            (void)pca9535PortLedPressShow(false, true, false);
            break;
        case eSYSTEM_DIAGNOSTIC_MODE:
        default:
            (void)pca9535PortLedPowerShow(true, false, false);
            (void)pca9535PortLedPressShow(true, false, false);
            (void)tm1651PortShowError(1U);
            break;
    }
}

static void systemApplyIndicatorsIfNeeded(void)
{
    if (!gSystemContext.indicatorsDirty) {
        return;
    }

    systemApplyIndicators(systemGetMode());
    gSystemContext.indicatorsDirty = false;
}

static void systemStopModeServices(eSystemMode mode)
{
    switch (mode) {
        case eSYSTEM_NORMAL_MODE:
            managerPowerStop();
            break;
        case eSYSTEM_UPDATE_MODE:
            managerUpdateStop();
            break;
        default:
            break;
    }
}

static void systemChangeMode(eSystemMode mode)
{
    eSystemMode currentMode;

    if (!systemIsValidMode(mode)) {
        return;
    }

    currentMode = repSystemGetMode();
    if (currentMode == mode) {
        return;
    }

    systemStopModeServices(currentMode);
    (void)repSystemSetMode(mode);
    gSystemContext.indicatorsDirty = true;
}

static bool systemInitDriversGpio(void)
{
    drvGpioInit();
    drvGpioWrite(DRVGPIO_EN_AUDIO, DRVGPIO_PIN_RESET);
    drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_RESET);
    drvGpioWrite(DRVGPIO_USB_SELECT, DRVGPIO_PIN_RESET);
    return true;
}

static bool systemInitDriversAdc(void)
{
    return (drvAdcInit(DRVADC_BAT) == DRV_STATUS_OK) &&
           (drvAdcInit(DRVADC_FORCE) == DRV_STATUS_OK) &&
           (drvAdcInit(DRVADC_DC) == DRV_STATUS_OK) &&
           (drvAdcInit(DRVADC_5V0) == DRV_STATUS_OK) &&
           (drvAdcInit(DRVADC_3V3) == DRV_STATUS_OK);
}

static bool systemInitDriversIic(void)
{
    return (drvIicInit(DRVIIC_BUS0) == DRV_STATUS_OK) &&
           (drvIicInit(DRVIIC_BUS1) == DRV_STATUS_OK);
}

static bool systemInitDriversSpi(void)
{
    return drvSpiInit(DRVSPI_BUS0) == DRV_STATUS_OK;
}

static bool systemInitDriversUart(void)
{
    return (drvUartInit(DRVUART_DEBUG) == DRV_STATUS_OK) &&
           (drvUartInit(DRVUART_AUDIO) == DRV_STATUS_OK);
}

static bool systemInitDriversUsb(void)
{
    return drvUsbInit(DRVUSB_DEV0) == DRV_STATUS_OK;
}

static bool systemInitModules(void)
{
    pca9535PortResetInit();
    pca9535PortResetWrite(true);
    pca9535PortDelayMs(PCA9535_PORT_RESET_ASSERT_MS);
    pca9535PortResetWrite(false);
    pca9535PortDelayMs(PCA9535_PORT_RESET_RELEASE_MS);

    return (pca9535PortInit() == DRV_STATUS_OK) &&
           (tm1651PortInit() == DRV_STATUS_OK) &&
           (w25qxxxInit(W25QXXX_DEV0) == DRV_STATUS_OK) &&
           (lsm6Init(LSM6_DEV0) == DRV_STATUS_OK);
}

static void systemInitEnterFault(void)
{
    gSystemContext.initStage = SYSTEM_INIT_STAGE_FAULT;
    systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
}

static bool systemCreateWorkerTasks(void)
{
    if (gSystemContext.workerTasksCreated) {
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

    gSystemContext.workerTasksCreated = true;
    return true;
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

void systemBootstrap(void)
{
    if (gSystemContext.isBootstrapped) {
        return;
    }

    repSystemReset();
    gSystemContext.isBootstrapped = true;
    gSystemContext.indicatorsDirty = true;
    gSystemContext.workerTasksCreated = false;
    gSystemContext.backgroundCounter = 0U;
    gSystemContext.initStage = SYSTEM_INIT_STAGE_GPIO;
    (void)repSystemSetMode(eSYSTEM_INIT_MODE);
    systemApplyIndicatorsIfNeeded();
}

bool systemInit(void)
{
    systemBootstrap();

    switch (gSystemContext.initStage) {
        case SYSTEM_INIT_STAGE_GPIO:
            if (!systemInitDriversGpio()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_ADC;
            return false;
        case SYSTEM_INIT_STAGE_ADC:
            if (!systemInitDriversAdc()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_IIC;
            return false;
        case SYSTEM_INIT_STAGE_IIC:
            if (!systemInitDriversIic()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_SPI;
            return false;
        case SYSTEM_INIT_STAGE_SPI:
            if (!systemInitDriversSpi()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_UART;
            return false;
        case SYSTEM_INIT_STAGE_UART:
            if (!systemInitDriversUart()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_USB;
            return false;
        case SYSTEM_INIT_STAGE_USB:
            if (!systemInitDriversUsb()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_MODULE;
            return false;
        case SYSTEM_INIT_STAGE_MODULE:
            if (!systemInitModules()) {
                systemInitEnterFault();
                return false;
            }
            gSystemContext.initStage = SYSTEM_INIT_STAGE_MANAGER;
            return false;
        case SYSTEM_INIT_STAGE_MANAGER:
            if (!managerInit()) {
                systemInitEnterFault();
                return false;
            }
            drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_SET);
            gSystemContext.initStage = SYSTEM_INIT_STAGE_COMPLETE;
            systemChangeMode(eSYSTEM_SELF_CHECK_MODE);
            return true;
        case SYSTEM_INIT_STAGE_COMPLETE:
            return true;
        case SYSTEM_INIT_STAGE_FAULT:
        default:
            return false;
    }
}

bool systemIsValidMode(eSystemMode mode)
{
    return repSystemIsValidMode(mode);
}

eSystemMode systemGetMode(void)
{
    return repSystemGetMode();
}

void systemSetMode(eSystemMode mode)
{
    systemChangeMode(mode);
}

void systemDefaultTaskStep(void)
{
    systemBootstrap();

    switch (systemGetMode()) {
        case eSYSTEM_INIT_MODE:
            (void)systemInit();
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            if (managerRunStartupSelfCheck()) {
                if (systemCreateWorkerTasks()) {
                    systemChangeMode(eSYSTEM_STANDBY_MODE);
                } else {
                    systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
                }
            } else {
                systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
            }
            break;
        case eSYSTEM_STANDBY_MODE:
            if (managerPowerStart()) {
                systemChangeMode(eSYSTEM_NORMAL_MODE);
            } else {
                systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
            }
            break;
        case eSYSTEM_UPDATE_MODE:
            if (!managerUpdateStart()) {
                systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
            }
            break;
        case eSYSTEM_NORMAL_MODE:
        case eSYSTEM_DIAGNOSTIC_MODE:
        default:
            break;
    }

    systemApplyIndicatorsIfNeeded();
}

void systemCommTaskStep(void)
{
}

void systemMemoryTaskStep(void)
{
    if (systemGetMode() != eSYSTEM_UPDATE_MODE) {
        return;
    }

    if (!managerUpdateStart()) {
        systemChangeMode(eSYSTEM_DIAGNOSTIC_MODE);
        systemApplyIndicatorsIfNeeded();
        return;
    }

    managerUpdateProcess();
}

void systemPowerTaskStep(void)
{
    if (systemGetMode() == eSYSTEM_NORMAL_MODE) {
        managerPowerProcess();
    }
}

void systemWirelessTaskStep(void)
{
}

void systemAudioTaskStep(void)
{
}

void systemBackgroundTaskStep(void)
{
    if (systemGetMode() == eSYSTEM_NORMAL_MODE) {
        gSystemContext.backgroundCounter = (uint16_t)((gSystemContext.backgroundCounter + 1U) % 1000U);
        (void)tm1651PortShowNumber3(gSystemContext.backgroundCounter);
        (void)pca9535PortLedLightNum((uint8_t)((gSystemContext.backgroundCounter % 8U) + 1U));
    }
}

/**************************End of file********************************/