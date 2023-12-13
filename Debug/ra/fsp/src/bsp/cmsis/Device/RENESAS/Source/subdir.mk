################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c \
../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c 

C_DEPS += \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.d \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.d 

OBJS += \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o \
./ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o 

SREC += \
ASL165_Test.srec 

MAP += \
ASL165_Test.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/%.o: ../ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/%.c
	$(file > $@.in,-mcpu=cortex-m23 -mthumb -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -gdwarf-4 -D_RENESAS_RA_ -D_RA_CORE=CM23 -D_RA_ORDINAL=1 -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/src" -I"." -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/fsp/inc" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/fsp/inc/api" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/fsp/inc/instances" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/fsp/src/rm_freertos_port" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/aws/FreeRTOS/FreeRTOS/Source/include" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra/arm/CMSIS_5/CMSIS/Core/Include" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra_gen" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra_cfg/fsp_cfg/bsp" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra_cfg/fsp_cfg" -I"C:/asl/ASL104E/ASL104E_HUB/ASL165_Test/ra_cfg/aws" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

