/************************************************************************************
* @file     : rep_config.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef PROJECT_REBUILDCPR_REP_CONFIG_H
#define PROJECT_REBUILDCPR_REP_CONFIG_H

#ifndef REP_MCU_PLATFORM_NONE
#define REP_MCU_PLATFORM_NONE 0
#endif

#ifndef REP_MCU_PLATFORM_STM32
#define REP_MCU_PLATFORM_STM32 1
#endif

#ifndef REP_RTOS_NONE
#define REP_RTOS_NONE 0
#endif

#ifndef REP_RTOS_FREERTOS
#define REP_RTOS_FREERTOS 1
#endif

#ifndef REP_RTOS_UCOSII
#define REP_RTOS_UCOSII 2
#endif

#ifndef REP_LOG_LEVEL_NONE
#define REP_LOG_LEVEL_NONE 0
#endif

#ifndef REP_LOG_LEVEL_ERROR
#define REP_LOG_LEVEL_ERROR 1
#endif

#ifndef REP_LOG_LEVEL_WARN
#define REP_LOG_LEVEL_WARN 2
#endif

#ifndef REP_LOG_LEVEL_INFO
#define REP_LOG_LEVEL_INFO 3
#endif

#ifndef REP_LOG_LEVEL_DEBUG
#define REP_LOG_LEVEL_DEBUG 4
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef enum eDrvStatus {
	DRV_STATUS_OK = 0,
	DRV_STATUS_INVALID_PARAM,
	DRV_STATUS_NOT_READY,
	DRV_STATUS_BUSY,
	DRV_STATUS_TIMEOUT,
	DRV_STATUS_NACK,
	DRV_STATUS_UNSUPPORTED,
	DRV_STATUS_ID_NOTMATCH,
	DRV_STATUS_ERROR,
} eDrvStatus;

#undef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM REP_MCU_PLATFORM_STM32

#undef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM REP_RTOS_FREERTOS

#ifndef REP_RTOS_CUBEMX_FREERTOS
#define REP_RTOS_CUBEMX_FREERTOS 5
#endif

#ifndef REP_RTOS_CMSIS_FREERTOS
#define REP_RTOS_CMSIS_FREERTOS REP_RTOS_CUBEMX_FREERTOS
#endif

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL REP_LOG_LEVEL_INFO
#endif

#ifndef DRVIIC_MAX
#define DRVIIC_MAX 2U
#endif

#ifndef DRVSPI_MAX
#define DRVSPI_MAX 1U
#endif

#ifndef DRVUART_MAX
#define DRVUART_MAX 2U
#endif

#ifndef DRVADC_MAX
#define DRVADC_MAX 5U
#endif

#ifndef DRVGPIO_MAX
#define DRVGPIO_MAX 6U
#endif

#ifndef DRVUSB_MAX
#define DRVUSB_MAX 1U
#endif

#ifndef DRVIIC_LOG_SUPPORT
#define DRVIIC_LOG_SUPPORT 0
#endif

#ifndef DRVSPI_LOG_SUPPORT
#define DRVSPI_LOG_SUPPORT 0
#endif

#ifndef DRVUART_LOG_SUPPORT
#define DRVUART_LOG_SUPPORT 0
#endif

#ifndef DRVADC_LOG_SUPPORT
#define DRVADC_LOG_SUPPORT 0
#endif

#ifndef DRVUSB_LOG_SUPPORT
#define DRVUSB_LOG_SUPPORT 0
#endif

#ifndef DRVIIC_CONSOLE_SUPPORT
#define DRVIIC_CONSOLE_SUPPORT 0
#endif

#ifndef DRVSPI_CONSOLE_SUPPORT
#define DRVSPI_CONSOLE_SUPPORT 0
#endif

#ifndef DRVUART_CONSOLE_SUPPORT
#define DRVUART_CONSOLE_SUPPORT 0
#endif

#ifndef DRVADC_CONSOLE_SUPPORT
#define DRVADC_CONSOLE_SUPPORT 0
#endif

#ifndef DRVUSB_CONSOLE_SUPPORT
#define DRVUSB_CONSOLE_SUPPORT 0
#endif

#endif
/**************************End of file********************************/
