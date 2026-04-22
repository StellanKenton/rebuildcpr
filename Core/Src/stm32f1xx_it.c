/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "SEGGER_RTT.h"
#include "../../User/bsp/bspuart.h"
/* USER CODE END Includes */

extern volatile uint32_t gSystemFaultTraceStage;
extern volatile uint32_t gDrvAdcFaultTraceStage;
extern volatile uint32_t gDrvAdcFaultTraceAdc;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint32_t gUsbIrqTraceCount = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static volatile uint32_t gHardFaultReported = 0U;
static char gHardFaultLineBuffer[128];

static char *faultAppendString(char *cursor, const char *text)
{
  while ((*text != '\0') && ((cursor - gHardFaultLineBuffer) < (ptrdiff_t)(sizeof(gHardFaultLineBuffer) - 1U)))
  {
    *cursor = *text;
    cursor++;
    text++;
  }

  return cursor;
}

static char *faultAppendHex32(char *cursor, uint32_t value)
{
  static const char lDigits[] = "0123456789ABCDEF";
  int32_t lShift;

  cursor = faultAppendString(cursor, "0x");
  for (lShift = 28; lShift >= 0; lShift -= 4)
  {
    if ((cursor - gHardFaultLineBuffer) >= (ptrdiff_t)(sizeof(gHardFaultLineBuffer) - 1U))
    {
      break;
    }

    *cursor = lDigits[(value >> (uint32_t)lShift) & 0x0FU];
    cursor++;
  }

  return cursor;
}

static void faultWriteLine(const char *label, uint32_t value)
{
  char *lCursor = gHardFaultLineBuffer;

  lCursor = faultAppendString(lCursor, "[fault] ");
  lCursor = faultAppendString(lCursor, label);
  lCursor = faultAppendString(lCursor, "=");
  lCursor = faultAppendHex32(lCursor, value);
  lCursor = faultAppendString(lCursor, "\r\n");
  *lCursor = '\0';
  SEGGER_RTT_WriteString(0, gHardFaultLineBuffer);
}

static bool faultStackPointerIsValid(const uint32_t *stack)
{
  uintptr_t lAddress = (uintptr_t)stack;

  return (lAddress >= 0x20000000UL) &&
         (lAddress <= (0x20010000UL - (8UL * sizeof(uint32_t))));
}

static void faultWriteStackFrame(const char *label, const uint32_t *stack)
{
  faultWriteLine(label, (uint32_t)(uintptr_t)stack);
  if (!faultStackPointerIsValid(stack))
  {
    SEGGER_RTT_WriteString(0, "[fault] stack frame invalid\r\n");
    return;
  }

  faultWriteLine("r0", stack[0]);
  faultWriteLine("r1", stack[1]);
  faultWriteLine("r2", stack[2]);
  faultWriteLine("r3", stack[3]);
  faultWriteLine("r12", stack[4]);
  faultWriteLine("stack_lr", stack[5]);
  faultWriteLine("stack_pc", stack[6]);
  faultWriteLine("stack_xpsr", stack[7]);
}

static void hardFaultReport(void)
{
  const uint32_t *lMsp = (const uint32_t *)(uintptr_t)__get_MSP();
  const uint32_t *lPsp = (const uint32_t *)(uintptr_t)__get_PSP();

  if (gHardFaultReported != 0U)
  {
    return;
  }

  gHardFaultReported = 1U;
  SEGGER_RTT_WriteString(0, "\r\n[fault] HardFault\r\n");
  faultWriteLine("MSP", (uint32_t)(uintptr_t)lMsp);
  faultWriteLine("PSP", (uint32_t)(uintptr_t)lPsp);
  faultWriteLine("ICSR", SCB->ICSR);
  faultWriteLine("SHCSR", SCB->SHCSR);
  faultWriteLine("CFSR", SCB->CFSR);
  faultWriteLine("HFSR", SCB->HFSR);
  faultWriteLine("DFSR", SCB->DFSR);
  faultWriteLine("AFSR", SCB->AFSR);
  faultWriteLine("MMFAR", SCB->MMFAR);
  faultWriteLine("BFAR", SCB->BFAR);
  
  faultWriteLine("sys_stage", gSystemFaultTraceStage);
  faultWriteLine("adc_stage", gDrvAdcFaultTraceStage);
  faultWriteLine("adc_idx", gDrvAdcFaultTraceAdc);
  SEGGER_RTT_WriteString(0, "[fault] MSP frame\r\n");
  faultWriteStackFrame("msp_sp", lMsp);
  SEGGER_RTT_WriteString(0, "[fault] PSP frame\r\n");
  faultWriteStackFrame("psp_sp", lPsp);
}

static void busFaultReport(void)
{
  const uint32_t *lMsp = (const uint32_t *)(uintptr_t)__get_MSP();
  const uint32_t *lPsp = (const uint32_t *)(uintptr_t)__get_PSP();

  if (gHardFaultReported != 0U)
  {
    return;
  }

  gHardFaultReported = 1U;
  SEGGER_RTT_WriteString(0, "\r\n[fault] BusFault\r\n");
  faultWriteLine("MSP", (uint32_t)(uintptr_t)lMsp);
  faultWriteLine("PSP", (uint32_t)(uintptr_t)lPsp);
  faultWriteLine("ICSR", SCB->ICSR);
  faultWriteLine("SHCSR", SCB->SHCSR);
  faultWriteLine("CFSR", SCB->CFSR);
  faultWriteLine("HFSR", SCB->HFSR);
  faultWriteLine("DFSR", SCB->DFSR);
  faultWriteLine("AFSR", SCB->AFSR);
  faultWriteLine("MMFAR", SCB->MMFAR);
  faultWriteLine("BFAR", SCB->BFAR);
  faultWriteLine("sys_stage", gSystemFaultTraceStage);
  faultWriteLine("adc_stage", gDrvAdcFaultTraceStage);
  faultWriteLine("adc_idx", gDrvAdcFaultTraceAdc);
  SEGGER_RTT_WriteString(0, "[fault] MSP frame\r\n");
  faultWriteStackFrame("msp_sp", lMsp);
  SEGGER_RTT_WriteString(0, "[fault] PSP frame\r\n");
  faultWriteStackFrame("psp_sp", lPsp);
}

static void usbTraceIrq(const char *pTag)
{
  char lBuffer[96];

  if (gUsbIrqTraceCount >= 16U)
  {
    return;
  }

  snprintf(
      lBuffer,
      sizeof(lBuffer),
      "[usb] %s #%lu ISTR=%04x EP0R=%04x CNTR=%04x DADDR=%04x\r\n",
      pTag,
      gUsbIrqTraceCount,
      USB->ISTR,
      USB->EP0R,
      USB->CNTR,
      USB->DADDR);
  SEGGER_RTT_WriteString(0, lBuffer);
  gUsbIrqTraceCount++;
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_FS;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;
extern I2C_HandleTypeDef hi2c2;
extern RTC_HandleTypeDef hrtc;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern TIM_HandleTypeDef htim7;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_uart4_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  hardFaultReport();

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  busFaultReport();

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC global interrupt.
  */
void RTC_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_IRQn 0 */

  /* USER CODE END RTC_IRQn 0 */
  HAL_RTCEx_RTCIRQHandler(&hrtc);
  /* USER CODE BEGIN RTC_IRQn 1 */

  /* USER CODE END RTC_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

  /* USER CODE END DMA1_Channel1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel2 global interrupt.
  */
void DMA1_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_IRQn 0 */

  /* USER CODE END DMA1_Channel2_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi1_rx);
  /* USER CODE BEGIN DMA1_Channel2_IRQn 1 */

  /* USER CODE END DMA1_Channel2_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel3 global interrupt.
  */
void DMA1_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel3_IRQn 0 */

  /* USER CODE END DMA1_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi1_tx);
  /* USER CODE BEGIN DMA1_Channel3_IRQn 1 */

  /* USER CODE END DMA1_Channel3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel4 global interrupt.
  */
void DMA1_Channel4_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel4_IRQn 0 */

  /* USER CODE END DMA1_Channel4_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_i2c2_tx);
  /* USER CODE BEGIN DMA1_Channel4_IRQn 1 */

  /* USER CODE END DMA1_Channel4_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel5 global interrupt.
  */
void DMA1_Channel5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel5_IRQn 0 */

  /* USER CODE END DMA1_Channel5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_i2c2_rx);
  /* USER CODE BEGIN DMA1_Channel5_IRQn 1 */

  /* USER CODE END DMA1_Channel5_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel6 global interrupt.
  */
void DMA1_Channel6_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel6_IRQn 0 */

  /* USER CODE END DMA1_Channel6_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart2_rx);
  /* USER CODE BEGIN DMA1_Channel6_IRQn 1 */

  /* USER CODE END DMA1_Channel6_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel7 global interrupt.
  */
void DMA1_Channel7_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel7_IRQn 0 */

  /* USER CODE END DMA1_Channel7_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart2_tx);
  /* USER CODE BEGIN DMA1_Channel7_IRQn 1 */

  /* USER CODE END DMA1_Channel7_IRQn 1 */
}

/**
  * @brief This function handles USB high priority or CAN TX interrupts.
  */
void USB_HP_CAN1_TX_IRQHandler(void)
{
  /* USER CODE BEGIN USB_HP_CAN1_TX_IRQn 0 */
  usbTraceIrq("USB_HP IRQ");

  /* USER CODE END USB_HP_CAN1_TX_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
  /* USER CODE BEGIN USB_HP_CAN1_TX_IRQn 1 */

  /* USER CODE END USB_HP_CAN1_TX_IRQn 1 */
}

/**
  * @brief This function handles USB low priority or CAN RX0 interrupts.
  */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 0 */
  usbTraceIrq("USB_LP IRQ");

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 1 */

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles I2C2 event interrupt.
  */
void I2C2_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_EV_IRQn 0 */

  /* USER CODE END I2C2_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c2);
  /* USER CODE BEGIN I2C2_EV_IRQn 1 */

  /* USER CODE END I2C2_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C2 error interrupt.
  */
void I2C2_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_ER_IRQn 0 */

  /* USER CODE END I2C2_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c2);
  /* USER CODE BEGIN I2C2_ER_IRQn 1 */

  /* USER CODE END I2C2_ER_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */
  bspUartHandleIrq(DRVUART_AUDIO);

  /* USER CODE END USART2_IRQn 1 */
}

/**
  * @brief This function handles UART4 global interrupt.
  */
void UART4_IRQHandler(void)
{
  /* USER CODE BEGIN UART4_IRQn 0 */

  /* USER CODE END UART4_IRQn 0 */
  HAL_UART_IRQHandler(&huart4);
  /* USER CODE BEGIN UART4_IRQn 1 */
  bspUartHandleIrq(DRVUART_WIFI);

  /* USER CODE END UART4_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
  /* USER CODE BEGIN TIM7_IRQn 0 */

  /* USER CODE END TIM7_IRQn 0 */
  HAL_TIM_IRQHandler(&htim7);
  /* USER CODE BEGIN TIM7_IRQn 1 */

  /* USER CODE END TIM7_IRQn 1 */
}

/**
  * @brief This function handles DMA2 channel3 global interrupt.
  */
void DMA2_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Channel3_IRQn 0 */

  /* USER CODE END DMA2_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_uart4_rx);
  /* USER CODE BEGIN DMA2_Channel3_IRQn 1 */

  /* USER CODE END DMA2_Channel3_IRQn 1 */
}

/**
  * @brief This function handles DMA2 channel4 and channel5 global interrupts.
  */
void DMA2_Channel4_5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Channel4_5_IRQn 0 */

  /* USER CODE END DMA2_Channel4_5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_uart4_tx);
  /* USER CODE BEGIN DMA2_Channel4_5_IRQn 1 */

  /* USER CODE END DMA2_Channel4_5_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
