set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARM_GNU_TOOLCHAIN_BIN_DIR "" CACHE PATH "Path to the Arm GNU Toolchain bin directory")

if(ARM_GNU_TOOLCHAIN_BIN_DIR)
  set(_arm_toolchain_prefix "${ARM_GNU_TOOLCHAIN_BIN_DIR}/")
else()
  set(_arm_toolchain_prefix "")
endif()

set(CMAKE_C_COMPILER "${_arm_toolchain_prefix}arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${_arm_toolchain_prefix}arm-none-eabi-gcc")
set(CMAKE_OBJCOPY "${_arm_toolchain_prefix}arm-none-eabi-objcopy" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE "${_arm_toolchain_prefix}arm-none-eabi-size" CACHE FILEPATH "size")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

