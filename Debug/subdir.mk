################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../main.cpp 

BCS += \
./main.bc 

CPP_DEPS += \
./main.d 


# Each subdirectory must supply rules for building sources it contributes
%.bc: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: LLVM Clang++'
	clang++ -D__GXX_EXPERIMENTAL_CXX0X__ -I/usr/local/include/ -O0 -emit-llvm -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


