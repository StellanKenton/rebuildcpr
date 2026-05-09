/***********************************************************************************
* @file     : sysmgr.c
* @brief    : Project-side system mode manager.
* @details  : Dispatches the current system mode and performs one-time BSP startup.
* @author   : rumi 
* @date     : 2026/04/11
* @version  : v0.0.1
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sysmgr.h"

#include "../../Core/Inc/adc.h"
#include "../../Core/Inc/dma.h"
#include "../../Core/Inc/gpio.h"
#include "../../Core/Inc/i2c.h"
#include "../../Core/Inc/iwdg.h"
#include "../../Core/Inc/rtc.h"
#include "../../Core/Inc/spi.h"
#include "../../Core/Inc/tim.h"
#include "../../Core/Inc/usart.h"

#include "drvgpio.h"
#include "../../rep/service/log/log.h"

#include "../manager/power/power.h"
#include "../manager/sensor/sensor.h"
#include "../manager/selfcheck/selfcheck.h"
#include "../manager/selfcheck/selfcheck_fault.h"
#include "../manager/memory/memory.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"
#include "systask.h"
#include "system.h"

#define SYSTEM_LOG_TAG "systemManager"

static bool gSystemInitModeCompleted = false;
static bool gSystemBspInitCompleted = false;

static void systemLogPowerupSelfCheckResult(void);
static void systemLogSelfCheckModuleResult(const char *moduleName, uint8_t payloadByte);
static void systemLogSelfCheckCprFaults(uint8_t payloadByte);
static void systemLogSelfCheckPowerFaults(uint8_t payloadByte);
static void systemLogSelfCheckAudioFaults(uint8_t payloadByte);
static void systemLogSelfCheckWirelessFaults(uint8_t payloadByte);
static void systemLogSelfCheckMemoryFaults(uint8_t payloadByte);
static void systemUpdateSelfCheckModuleSummary(void);
static void systemLogResetCause(void);
static void systemEnablePreciseFaultDebug(void)
{
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
#if defined(SCB_ACTLR_DISDEFWBUF_Msk)
    SCB->ACTLR |= SCB_ACTLR_DISDEFWBUF_Msk;
#elif defined(SCnSCB_ACTLR_DISDEFWBUF_Msk)
    SCnSCB->ACTLR |= SCnSCB_ACTLR_DISDEFWBUF_Msk;
#endif
}

static void systemLogSelfCheckModuleResult(const char *moduleName, uint8_t payloadByte)
{
    if ((payloadByte & SELF_CHECK_FAULT_MODULE_PASS) != 0U) {
        LOG_I(SYSTEM_LOG_TAG, "selfcheck %s pass payload=0x%02X", moduleName, (unsigned int)payloadByte);
    } else {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck %s fail payload=0x%02X", moduleName, (unsigned int)payloadByte);
    }
}

static void systemLogSelfCheckCprFaults(uint8_t payloadByte)
{
    systemLogSelfCheckModuleResult("cpr", payloadByte);
    if ((payloadByte & SELF_CHECK_FAULT_CPR_ACC_INIT) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck cpr fault E01 H: accelerometer init failed");
    }
    if ((payloadByte & SELF_CHECK_FAULT_CPR_RTC) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck cpr fault E02 M: rtc time invalid");
    }
}

static void systemLogSelfCheckPowerFaults(uint8_t payloadByte)
{
    systemLogSelfCheckModuleResult("power", payloadByte);
    if ((payloadByte & SELF_CHECK_FAULT_POWER_3V3_HIGH) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck power fault E11 H: 3.3V high");
    }
    if ((payloadByte & SELF_CHECK_FAULT_POWER_3V3_LOW) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck power fault E12 H: 3.3V low");
    }
    if ((payloadByte & SELF_CHECK_FAULT_POWER_5V_HIGH) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck power fault E13 H: 5V high");
    }
    if ((payloadByte & SELF_CHECK_FAULT_POWER_5V_LOW) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck power fault E14 H: 5V low");
    }
    if ((payloadByte & SELF_CHECK_FAULT_POWER_DC_HIGH) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck power fault E15 H: DC input high");
    }
}

static void systemLogSelfCheckAudioFaults(uint8_t payloadByte)
{
    systemLogSelfCheckModuleResult("audio", payloadByte);
    if ((payloadByte & SELF_CHECK_FAULT_AUDIO_COMM) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck audio fault E21 M: audio communication failed");
    }
    if ((payloadByte & SELF_CHECK_FAULT_AUDIO_MUSIC_NUM) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck audio fault E22 L: audio music number abnormal");
    }
}

static void systemLogSelfCheckWirelessFaults(uint8_t payloadByte)
{
    systemLogSelfCheckModuleResult("wireless", payloadByte);
    if ((payloadByte & SELF_CHECK_FAULT_WIRELESS_INIT) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck wireless fault E31 M: wireless init failed");
    }
}

static void systemLogSelfCheckMemoryFaults(uint8_t payloadByte)
{
    systemLogSelfCheckModuleResult("memory", payloadByte);
    if ((payloadByte & SELF_CHECK_FAULT_MEMORY_INIT) != 0U) {
        LOG_W(SYSTEM_LOG_TAG, "selfcheck memory fault E41 H: memory init failed");
    }
}

static void systemLogPowerupSelfCheckResult(void)
{
    stSelfCheckFaultPayload lCurrentPayload;
    stSelfCheckFaultPayload lWindowPayload;
    const stSelfCheckSummary *lSummary;
    uint32_t lBootRtcTime;
    bool lBootRtcReady;

    selfCheckFaultGetCurrentPayload(&lCurrentPayload);
    selfCheckFaultGetWindowPayload(&lWindowPayload);
    lSummary = selfCheckGetSummary();
    lBootRtcReady = selfCheckFaultGetBootRtcTime(&lBootRtcTime);

    LOG_I(SYSTEM_LOG_TAG,
          "powerup selfcheck result pass=%d run=%d boot_rtc_ready=%d boot_rtc=0x%08lX cur=%02X/%02X/%02X/%02X/%02X win=%02X/%02X/%02X/%02X/%02X",
          (lSummary != NULL) ? (int)lSummary->isPassed : 0,
          (lSummary != NULL) ? (int)lSummary->hasRun : 0,
          (int)lBootRtcReady,
          (unsigned long)lBootRtcTime,
          (unsigned int)lCurrentPayload.cpr,
          (unsigned int)lCurrentPayload.power,
          (unsigned int)lCurrentPayload.audio,
          (unsigned int)lCurrentPayload.wireless,
          (unsigned int)lCurrentPayload.memory,
          (unsigned int)lWindowPayload.cpr,
          (unsigned int)lWindowPayload.power,
          (unsigned int)lWindowPayload.audio,
          (unsigned int)lWindowPayload.wireless,
          (unsigned int)lWindowPayload.memory);

    LOG_I(SYSTEM_LOG_TAG, "powerup selfcheck current detail");
    systemLogSelfCheckCprFaults(lCurrentPayload.cpr);
    systemLogSelfCheckPowerFaults(lCurrentPayload.power);
    systemLogSelfCheckAudioFaults(lCurrentPayload.audio);
    systemLogSelfCheckWirelessFaults(lCurrentPayload.wireless);
    systemLogSelfCheckMemoryFaults(lCurrentPayload.memory);
}

static void systemLogResetCause(void)
{
    uint32_t lCsr = RCC->CSR;

    LOG_I(SYSTEM_LOG_TAG,
          "reset csr=0x%08lX pin=%u por=%u sftrst=%u iwdg=%u wwdg=%u lpwr=%u",
          (unsigned long)lCsr,
          __HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) ? 1U : 0U,
          __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) ? 1U : 0U,
          __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) ? 1U : 0U,
          __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? 1U : 0U,
          __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? 1U : 0U,
          __HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) ? 1U : 0U);

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/**
* @brief : Run the generated STM32 BSP initialization sequence.
* @param : None
* @return: None
**/
static void systemInitBsp(void)
{
    if (gSystemBspInitCompleted) {
        return;
    }

    systemEnablePreciseFaultDebug();
    systemLogResetCause();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_IWDG_Init();
    (void)HAL_IWDG_Refresh(&hiwdg);
    MX_RTC_Init();
    MX_SPI1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_TIM7_Init();
    MX_UART4_Init();
    MX_USART2_UART_Init();

    HAL_UART_MspInit(&huart4);
    HAL_UART_MspInit(&huart2);
    gSystemBspInitCompleted = true;
}

/**
* @brief : Run one-time BSP and basic manager initialization.
* @param : None
* @return: true when the required initialization steps succeed.
**/
static bool systemModuleInit(void)
{
    bool lIsReady = true;

    drvGpioInit();

    lIsReady = selfCheckInit() && lIsReady;
    selfCheckFaultInit();
    LOG_I(SYSTEM_LOG_TAG, "selfcheck init %s", lIsReady ? "ok" : "fail");
    (void)HAL_IWDG_Refresh(&hiwdg);

    if (pca9535PortInit() == DRV_STATUS_OK) {
        selfCheckSetExpanderResult(true);
        (void)pca9535PortLedOff();
        LOG_I(SYSTEM_LOG_TAG, "pca9535 init ok");
    } else {
        selfCheckSetExpanderResult(false);
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "pca9535 init fail");
    }
    (void)HAL_IWDG_Refresh(&hiwdg);

    if (tm1651PortInit() == DRV_STATUS_OK) {
        selfCheckSetDisplayResult(true);
        (void)tm1651PortClearDisplay();
        LOG_I(SYSTEM_LOG_TAG, "tm1651 init ok");
    } else {
        selfCheckSetDisplayResult(false);
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "tm1651 init fail");
    }
    (void)HAL_IWDG_Refresh(&hiwdg);

    return lIsReady;
}

static void systemUpdateSelfCheckModuleSummary(void)
{
    selfCheckSetPowerResult(powerIsReady());
    selfCheckSetMotionResult(sensorIsReady());
    selfCheckSetFlashResult(memoryIsReady());
    selfCheckSetUpdateResult(true);
}

/**
* @brief : Handle the system init mode.
* @param : None
* @return: None
**/
static void systemInitMode(void)
{
    if (gSystemInitModeCompleted) {
        return;
    }
    LOG_I(SYSTEM_LOG_TAG, "&&&&&&&&&&&&&&&&& SYSTEM POWER UP &&&&&&&&&&&&&&&&&");
    LOG_I(SYSTEM_LOG_TAG, "System initialized.");
    LOG_I(SYSTEM_LOG_TAG, "Firmware: %s, Version: %s, Hardware: %s", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
    systemInitBsp();
    
    if (!systemModuleInit()) {
        LOG_E(SYSTEM_LOG_TAG, "system init mode blocked");
        return;
    }

    gSystemInitModeCompleted = true;
    LOG_I(SYSTEM_LOG_TAG, "switch to powerup selfcheck mode");
    systemSetMode(eSYSTEM_POWERUP_SELFCHECK_MODE);
}

/**
* @brief : Handle the power-up self-check mode.
* @param : None
* @return: None
**/
static void systemPowerupSelfCheckMode(void)
{
    systemSetMode(eSYSTEM_SELF_CHECK_MODE);
    batholdon();
    if (!systaskCreateWorkerTasks()) {
        LOG_E(SYSTEM_LOG_TAG, "worker task create failed");
        return;
    }
}

/**
* @brief : Handle the standby mode.
* @param : None
* @return: None
**/
static void systemStandbyMode(void)
{
    systemSetMode(eSYSTEM_NORMAL_MODE);
}

/**
* @brief : Handle the normal mode.
* @param : None
* @return: None
**/
static void systemNormalMode(void)
{
    
}

/**
* @brief : Handle the manual self-check mode.
* @param : None
* @return: None
**/
static void systemSelfCheckMode(void)
{
    static bool lSelfCheckCompleted = false;

    if (!lSelfCheckCompleted) {
        systemUpdateSelfCheckModuleSummary();
        lSelfCheckCompleted = selfCheckFaultRunStartupWindow(SELF_CHECK_FAULT_STARTUP_DURATION_MS);
        if (lSelfCheckCompleted) {
            systemUpdateSelfCheckModuleSummary();
            (void)selfCheckCommit();
            systemLogPowerupSelfCheckResult();
        }
    }

    if(lSelfCheckCompleted && memoryIsReady()) {
        systemSetMode(eSYSTEM_STANDBY_MODE);
    }
}

/**
* @brief : Handle the update mode.
* @param : None
* @return: None
**/
static void systemUpdateMode(void)
{
}

/**
* @brief : Handle the diagnostic mode.
* @param : None
* @return: None
**/
static void systemDiagnosticMode(void)
{
}

/**
* @brief : Handle the end-of-line mode.
* @param : None
* @return: None
**/
static void systemEolMode(void)
{
}

/**
* @brief : Dispatch the system mode handler.
* @param : None
* @return: None
**/
void systemManagerRun(void)
{
    switch (systemGetMode()) {
        case eSYSTEM_INIT_MODE:
            systemInitMode();
            break;
        case eSYSTEM_POWERUP_SELFCHECK_MODE:
            systemPowerupSelfCheckMode();
            break;
        case eSYSTEM_STANDBY_MODE:
            systemStandbyMode();
            break;
        case eSYSTEM_NORMAL_MODE:
            systemNormalMode();
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            systemSelfCheckMode();
            break;
        case eSYSTEM_UPDATE_MODE:
            systemUpdateMode();
            break;
        case eSYSTEM_DIAGNOSTIC_MODE:
            systemDiagnosticMode();
            break;
        case eSYSTEM_EOL_MODE:
            systemEolMode();
            break;
        default:
            break;
    }
}

/**************************End of file********************************/
