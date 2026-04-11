cmake_minimum_required(VERSION 3.22)

project(CprSensor C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(PROJECT_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/linker/stm32f103xe_flash.ld)

set(PROJECT_SOURCES
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
    User/port/drviic_port.c
    User/port/drvspi_port.c
    User/port/drvuart_port.c
    User/port/drvgpio_port.c
    User/port/drvadc_port.c
    User/port/drvusb_port.c
    User/port/w25qxxx_port.c
    User/port/mpu6050_port.c
    User/port/pca9535_port.c
    User/port/tm1651_port.c
    User/port/lsm6_port.c
    User/port/rtos_port.c
    rep/service/lifecycle/lifecycle.c
    User/manager/manager.c
    User/manager/power/power.c
    User/manager/selfcheck/selfcheck.c
    User/manager/update/update.c
    User/system/system.c
    User/system/systask.c
    rep/driver/drviic/drviic.c
    rep/driver/drvspi/drvspi.c
    rep/driver/drvuart/drvuart.c
    rep/driver/drvgpio/drvgpio.c
    rep/driver/drvadc/drvadc.c
    rep/driver/drvusb/drvusb.c
    rep/service/rtos/rtos.c
    rep/module/w25qxxx/w25qxxx.c
    rep/module/mpu6050/mpu6050.c
    rep/module/pca9535/pca9535.c
    rep/module/tm1651/tm1651.c
    rep/module/lsm6/lsm6.c
    rep/tools/ringbuffer/ringbuffer.c
    Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xe.s
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/User
    ${CMAKE_CURRENT_SOURCE_DIR}/User/manager
    ${CMAKE_CURRENT_SOURCE_DIR}/User/system
    ${CMAKE_CURRENT_SOURCE_DIR}/User/port
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/App
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_DEVICE/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drviic
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvspi
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvuart
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvgpio
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvadc
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvusb
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/driver/drvanlogiic
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/module/mpu6050
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/module/w25qxxx
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/module/pca9535
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/module/tm1651
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/module/lsm6
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/service/lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/service/rtos
    ${CMAKE_CURRENT_SOURCE_DIR}/rep/tools/ringbuffer
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xE
    DRVGPIO_LOG_SUPPORT=0
    DRVGPIO_CONSOLE_SUPPORT=0
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-sign-compare
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
    -T${PROJECT_LINKER_SCRIPT}
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--start-group
    -lc
    -lm
    -Wl,--end-group
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "CprSensor"
    SUFFIX ".elf"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/CprSensor.hex
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/CprSensor.bin
    COMMAND ${CMAKE_SIZE_UTIL} $<TARGET_FILE:${PROJECT_NAME}>
    VERBATIM
)