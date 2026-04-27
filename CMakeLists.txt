cmake_minimum_required(VERSION 3.22)

project(CprSensor C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(TARGET_NAME CprSensor)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/linker/STM32F103RETx_FLASH.ld)

add_executable(${TARGET_NAME}
  Core/Startup/startup_stm32f103xe_gcc.s
  Core/Src/main.c
  Core/Src/gpio.c
  Core/Src/freertos.c
  Core/Src/adc.c
  Core/Src/dma.c
  Core/Src/i2c.c
  Core/Src/iwdg.c
  Core/Src/rtc.c
  Core/Src/spi.c
  Core/Src/tim.c
  Core/Src/usart.c
  Core/Src/stm32f1xx_it.c
  Core/Src/stm32f1xx_hal_msp.c
  Core/Src/stm32f1xx_hal_timebase_tim.c
  Core/Src/system_stm32f1xx.c
  USB_DEVICE/App/usb_device.c
  USB_DEVICE/App/usbd_desc.c
  USB_DEVICE/App/usbd_cdc_if.c
  USB_DEVICE/Target/usbd_conf.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
  Middlewares/Third_Party/FreeRTOS/Source/croutine.c
  Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
  Middlewares/Third_Party/FreeRTOS/Source/list.c
  Middlewares/Third_Party/FreeRTOS/Source/queue.c
  Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
  Middlewares/Third_Party/FreeRTOS/Source/tasks.c
  Middlewares/Third_Party/FreeRTOS/Source/timers.c
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
  Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
  Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
  Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
  Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
  Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
  SEGGER/SEGGER_RTT.c
  User/manager/power/power.c
  User/manager/selfcheck/selfcheck.c
  User/manager/memory/memory.c
  User/manager/memory/memory_debug.c
  User/manager/update/update.c
  User/manager/wireless/wireless.c
  User/system/sysmgr.c
  User/system/systask.c
  User/system/system.c
  User/system/system_debug.c
  User/bsp/bsp_rtt.c
  User/bsp/bspanlogiic.c
  User/bsp/bspgpio.c
  User/bsp/bspiic.c
  User/bsp/bspspi.c
  User/bsp/bspuart.c
  User/bsp/bspusb.c
  User/port/drvadc_port.c
  User/port/drvgpio_port.c
  User/port/drviic_port.c
  User/port/drvspi_port.c
  User/port/drvuart_port.c
  User/port/fc41dport.c
  User/port/drvusb_port.c
  User/port/log_port.c
  User/port/pca9535_port.c
  User/port/rtos_port.c
  User/port/tm1651_port.c
  User/port/drvanlogiic_port.c
  User/port/gd25qxxx_port.c
  User/port/vfs_littlefs_port.c
  rep/service/vfs/vfs.c
  rep/service/vfs/vfs_littlefs.c
  rep/service/vfs/vfs_debug.c
  rep/lib/littlefs/lfs.c
  rep/lib/littlefs/lfs_util.c
  rep/module/fc41d/fc41d.c
  rep/module/fc41d/fc41d_ctrl.c
  rep/module/fc41d/fc41d_data.c
  rep/comm/flowparser/flowparser.c
  rep/comm/flowparser/flowparser_stream.c
  rep/driver/drvadc/drvadc.c
  rep/driver/drvanlogiic/drvanlogiic.c
  rep/driver/drvgpio/drvgpio.c
  rep/driver/drviic/drviic.c
  rep/driver/drvspi/drvspi.c
  rep/driver/drvuart/drvuart.c
  rep/driver/drvusb/drvusb.c
  rep/module/gd25qxxx/gd25qxxx.c
  rep/module/pca9535/pca9535.c
  rep/module/tm1651/tm1651.c
  rep/tools/ringbuffer/ringbuffer.c
  rep/comm/frameparser/framepareser.c
  rep/service/log/console.c
  rep/service/log/log.c
  rep/service/rtos/rtos.c
  rep/driver/drvadc/drvadc_debug.c
  rep/driver/drvanlogiic/drvanlogiic_debug.c
  rep/driver/drvgpio/drvgpio_debug.c
  rep/driver/drviic/drviic_debug.c
  rep/driver/drvspi/drvspi_debug.c
  rep/driver/drvuart/drvuart_debug.c
)

target_include_directories(${TARGET_NAME} PRIVATE
  .
  Core/Inc
  User
  User/manager
  User/system
  User/port
  User/bsp
  USB_DEVICE/App
  USB_DEVICE/Target
  Drivers/STM32F1xx_HAL_Driver/Inc
  Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
  Middlewares/Third_Party/FreeRTOS/Source/include
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
  Middlewares/ST/STM32_USB_Device_Library/Core/Inc
  Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
  Drivers/CMSIS/Device/ST/STM32F1xx/Include
  Drivers/CMSIS/Include
  SEGGER
  rep
  rep/comm/flowparser
  rep/comm/frameparser
  rep/driver/drvadc
  rep/driver/drvanlogiic
  rep/driver/drvgpio
  rep/driver/drviic
  rep/driver/drvspi
  rep/driver/drvuart
  rep/driver/drvusb
  rep/lib/littlefs
  rep/module/fc41d
  rep/module/gd25qxxx
  rep/module/pca9535
  rep/module/tm1651
  rep/service/log
  rep/service/rtos
  rep/service/vfs
  rep/tools/ringbuffer
)

target_compile_definitions(${TARGET_NAME} PRIVATE
  USE_HAL_DRIVER
  STM32F103xE
  DRVGPIO_LOG_SUPPORT=0
  DRVGPIO_CONSOLE_SUPPORT=0
)

target_compile_options(${TARGET_NAME} PRIVATE
  -mcpu=cortex-m3
  -mthumb
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wno-unused-parameter
  $<$<COMPILE_LANGUAGE:C>:-fno-common>
)

target_link_options(${TARGET_NAME} PRIVATE
  -mcpu=cortex-m3
  -mthumb
  -T${LINKER_SCRIPT}
  -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  --specs=nano.specs
  --specs=nosys.specs
)

set_target_properties(${TARGET_NAME} PROPERTIES
  OUTPUT_NAME ${TARGET_NAME}
  SUFFIX ".elf"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin
)
