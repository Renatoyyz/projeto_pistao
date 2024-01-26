cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(regua_encoder_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(regua_encoder encoder.c)

pico_enable_stdio_usb(regua_encoder 1)
pico_enable_stdio_uart(regua_encoder 1)

# Inclua o suporte ao ADC
target_link_libraries(regua_encoder PRIVATE pico_stdlib hardware_adc hardware_pwm pico_multicore)

pico_add_extra_outputs(regua_encoder)
