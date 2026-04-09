/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 64 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for commTask */
osThreadId_t commTaskHandle;
const osThreadAttr_t commTask_attributes = {
  .name = "commTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for memorytask */
osThreadId_t memorytaskHandle;
const osThreadAttr_t memorytask_attributes = {
  .name = "memorytask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow5,
};
/* Definitions for powertask */
osThreadId_t powertaskHandle;
const osThreadAttr_t powertask_attributes = {
  .name = "powertask",
  .stack_size = 64 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for wirelessTask */
osThreadId_t wirelessTaskHandle;
const osThreadAttr_t wirelessTask_attributes = {
  .name = "wirelessTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for audioTask */
osThreadId_t audioTaskHandle;
const osThreadAttr_t audioTask_attributes = {
  .name = "audioTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for backgroundTask */
osThreadId_t backgroundTaskHandle;
const osThreadAttr_t backgroundTask_attributes = {
  .name = "backgroundTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for testQueue */
osMessageQueueId_t testQueueHandle;
const osMessageQueueAttr_t testQueue_attributes = {
  .name = "testQueue"
};
/* Definitions for testMutex */
osMutexId_t testMutexHandle;
const osMutexAttr_t testMutex_attributes = {
  .name = "testMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void DefaultTask(void *argument);
void CommTask(void *argument);
void MemoryTask(void *argument);
void PowerTask(void *argument);
void WirelessTask(void *argument);
void AudioTask(void *argument);
void BackGroudTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of testMutex */
  testMutexHandle = osMutexNew(&testMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of testQueue */
  testQueueHandle = osMessageQueueNew (1, sizeof(uint16_t), &testQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(DefaultTask, NULL, &defaultTask_attributes);

  /* creation of commTask */
  commTaskHandle = osThreadNew(CommTask, NULL, &commTask_attributes);

  /* creation of memorytask */
  memorytaskHandle = osThreadNew(MemoryTask, NULL, &memorytask_attributes);

  /* creation of powertask */
  powertaskHandle = osThreadNew(PowerTask, NULL, &powertask_attributes);

  /* creation of wirelessTask */
  wirelessTaskHandle = osThreadNew(WirelessTask, NULL, &wirelessTask_attributes);

  /* creation of audioTask */
  audioTaskHandle = osThreadNew(AudioTask, NULL, &audioTask_attributes);

  /* creation of backgroundTask */
  backgroundTaskHandle = osThreadNew(BackGroudTask, NULL, &backgroundTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_DefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_DefaultTask */
void DefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN DefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END DefaultTask */
}

/* USER CODE BEGIN Header_CommTask */
/**
* @brief Function implementing the commTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CommTask */
void CommTask(void *argument)
{
  /* USER CODE BEGIN CommTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END CommTask */
}

/* USER CODE BEGIN Header_MemoryTask */
/**
* @brief Function implementing the memorytask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_MemoryTask */
void MemoryTask(void *argument)
{
  /* USER CODE BEGIN MemoryTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END MemoryTask */
}

/* USER CODE BEGIN Header_PowerTask */
/**
* @brief Function implementing the powertask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_PowerTask */
void PowerTask(void *argument)
{
  /* USER CODE BEGIN PowerTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END PowerTask */
}

/* USER CODE BEGIN Header_WirelessTask */
/**
* @brief Function implementing the wirelessTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_WirelessTask */
void WirelessTask(void *argument)
{
  /* USER CODE BEGIN WirelessTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END WirelessTask */
}

/* USER CODE BEGIN Header_AudioTask */
/**
* @brief Function implementing the audioTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AudioTask */
void AudioTask(void *argument)
{
  /* USER CODE BEGIN AudioTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END AudioTask */
}

/* USER CODE BEGIN Header_BackGroudTask */
/**
* @brief Function implementing the backgroundTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BackGroudTask */
void BackGroudTask(void *argument)
{
  /* USER CODE BEGIN BackGroudTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END BackGroudTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
