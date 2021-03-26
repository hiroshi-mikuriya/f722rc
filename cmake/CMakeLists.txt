cmake_minimum_required(VERSION 3.6)
 
set(CMAKE_SYSTEM_NAME Generic)

##########
# architecture
##########
set(CPU       cortex-m7)
set(CHIP      STM32F722)
set(LDSCRIPT  STM32F722RCTX_FLASH.ld)
set(FPU       -mfpu=fpv5-sp-d16)
set(FLOAT_ABI -mfloat-abi=hard)

##########
# project name
##########
project(f722rc)

set(EXE_NAME ${PROJECT_NAME}_${CHIP})
set(ELF_FILE ${EXE_NAME}.elf)
set(HEX_FILE ${EXE_NAME}.hex)
set(BIN_FILE ${EXE_NAME}.bin)
set(MAP_FILE ${EXE_NAME}.map)

##########
# compiler
##########
execute_process(COMMAND which arm-none-eabi-gcc     OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_C_COMPILER)
execute_process(COMMAND which arm-none-eabi-gcc     OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_ASM_COMPILER)
execute_process(COMMAND which arm-none-eabi-g++     OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_CXX_COMPILER)
execute_process(COMMAND which arm-none-eabi-g++     OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_EXE_LINKER)
execute_process(COMMAND which arm-none-eabi-objcopy OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE OBJCOPY)
execute_process(COMMAND which arm-none-eabi-size    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE SIZE)

##########
# compiler options
##########
add_compile_options(-mcpu=${CPU})
add_compile_options(-mthumb)
add_compile_options(${FPU})
add_compile_options(${FLOAT_ABI})
add_compile_options(-O2)
add_compile_options(-Wall)
add_compile_options(-fdata-sections)
add_compile_options(-ffunction-sections)
add_compile_options(-MMD)
add_compile_options(-MP)

set(CMAKE_C_FLAGS -std=gnu99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS -fno-use-cxa-atexit)
set(CMAKE_AS_FLAGS "-x assembler-with-cpp")

add_link_options(-mcpu=${CPU})
add_link_options(-mthumb)
add_link_options(${FPU})
add_link_options(${FLOAT_ABI})
add_link_options(--specs=nosys.specs)
add_link_options(-T${CMAKE_CURRENT_SOURCE_DIR}/${LDSCRIPT})
add_link_options(-lc)
add_link_options(-lm)
add_link_options(-lnosys)
add_link_options(-Wl,-Map=${MAP_FILE},--cref)
add_link_options(-Wl,--gc-sections)

enable_language(ASM)

##########
# defines
##########
add_definitions(-D${CHIP}xx)
add_definitions(-DUSE_FULL_LL_DRIVER)
add_definitions(-DUSE_HAL_DRIVER)

##########
# directory name
##########
set(ROOT ..)
set(CORE ${ROOT}/Core)
set(DRIVER ${ROOT}/Drivers/STM32F7xx_HAL_Driver)
set(CMSIS ${ROOT}/Drivers/CMSIS)
set(RTOS ${ROOT}/Middlewares/Third_Party/FreeRTOS/Source)

##########
# header include path
##########
include_directories(
	${CORE}/fx
	${CORE}/user
	${CORE}/Inc
	${DRIVER}/Inc
	${CMSIS}/Device/ST/STM32F7xx/Include
	${CMSIS}/Include
	${RTOS}/include
	${RTOS}/CMSIS_RTOS
	${RTOS}/portable/GCC/ARM_CM7/r0p1
)

##########
# source files
##########
file(GLOB SRCS
	${CORE}/fx/*.cpp
	${CORE}/user/*.c
	${CORE}/user/*.cpp
	${CORE}/Src/*.c
	${CORE}/Src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f722xx.s
	${DRIVER}/Src/*.c
	${RTOS}/*.c
	${RTOS}/CMSIS_RTOS/*.c
	${RTOS}/portable/MemMang/*.c
	${RTOS}/portable/GCC/ARM_CM7/r0p1/*.c
)

##########
# products
##########
add_executable(${ELF_FILE} ${SRCS})

add_custom_command(TARGET ${ELF_FILE} POST_BUILD
	COMMAND ${OBJCOPY} -Obinary ${ELF_FILE} ${BIN_FILE}
	COMMAND ${OBJCOPY} -Oihex   ${ELF_FILE} ${HEX_FILE}
	COMMAND ${SIZE} --format=berkeley ${ELF_FILE} ${HEX_FILE}
	COMMENT "Invoking: Cross ARM GNU Print Size"
)
