cmake_minimum_required(VERSION 3.13)
# Configurações básicas
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico w CACHE STRING "Board Type")
include(pico_sdk_import.cmake)
# Define o projeto
project(Coletor_Pixels C CXX ASM)
pico_sdk_init()
# Adiciona os arquivos do programa
add_executable(Coletor_Pixels
    main.c
    libs/Matriz_Bibliotecas/matriz_led.c   
    libs/Display_Bibliotecas/ssd1306.c
)
# Habilita comunicação serial
pico_enable_stdio_uart(Coletor_Pixels 1)
pico_enable_stdio_usb(Coletor_Pixels 1)  # Ativa comunicação USB
# Vincula as bibliotecas necessárias
target_link_libraries(Coletor_Pixels PRIVATE
    pico_stdlib      # Biblioteca padrão do Pico
    hardware_i2c     # Suporte para comunicação I2C (Display)
    hardware_adc     # Suporte para ADC (Joystick)
    hardware_pio     # Suporte para PIO 
    hardware_pwm     # Suporte PWM
    m                #Math
)

# Adiciona saídas extras (binário, UF2, etc.)
pico_add_extra_outputs(Coletor_Pixels)
