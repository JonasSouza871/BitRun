# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

# Configurações básicas
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configuração do SDK do Raspberry Pi Pico
include(pico_sdk_import.cmake)

# Define o projeto
project(Coletor_Pixels C CXX ASM)

# Inicializa o SDK do Pico
pico_sdk_init()

# Adiciona os arquivos do programa
add_executable(Coletor_Pixels
    main.c
    libs/Matriz_Bibliotecas/matriz_led.c   # Mantido para uso futuro
    libs/Display_Bibliotecas/ssd1306.c
    # funcoes_graficas.c # Removido - não fornecido/necessário agora
)

# Adiciona os diretórios das bibliotecas customizadas ao include path
target_include_directories(Coletor_Pixels PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/libs/Display_Bibliotecas
    ${CMAKE_CURRENT_LIST_DIR}/libs/Matriz_Bibliotecas  # Corrected typo: Matriz instead of atriz
    ${CMAKE_CURRENT_LIST_DIR}/libs/Matriz_Bibliotecas/generated # For the PIO header
)

# Gera o cabeçalho PIO para os LEDs WS2812 (Mantido para uso futuro)

# A inclusão do diretório gerado já está feita acima em target_include_directories

# Habilita comunicação serial
pico_enable_stdio_uart(Coletor_Pixels 1)
pico_enable_stdio_usb(Coletor_Pixels 1)  # Ativa comunicação USB

# Vincula as bibliotecas necessárias
target_link_libraries(Coletor_Pixels PRIVATE
    pico_stdlib      # Biblioteca padrão do Pico
    hardware_i2c     # Suporte para comunicação I2C (Display)
    hardware_adc     # Suporte para ADC (Joystick)
    hardware_pio     # Suporte para PIO (Mantido para Matriz WS2812 futura)
    hardware_pwm   # Removido - não usado atualmente
    m                # Biblioteca matemática (pode ser útil)
)

# Adiciona saídas extras (binário, UF2, etc.)
pico_add_extra_outputs(Coletor_Pixels)