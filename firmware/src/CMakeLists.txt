add_executable(fw main.cpp)
target_link_libraries(fw pico_stdlib hardware_i2c hardware_gpio hardware_pwm
                      hardware_adc)
target_compile_options(fw PRIVATE -Wall -Wextra)

pico_enable_stdio_usb(fw 1)
pico_enable_stdio_uart(fw 0)
pico_add_extra_outputs(fw)
