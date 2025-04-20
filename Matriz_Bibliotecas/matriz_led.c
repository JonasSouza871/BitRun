#include "matriz_led.h"

// Definições dos padrões de números (5x5)
const bool numero_0[NUM_PIXELS] = {
    0,1,1,1,0,
    1,0,0,0,1,
    1,0,0,0,1,
    1,0,0,0,1,
    0,1,1,1,0
};

const bool numero_1[NUM_PIXELS] = {
    0,0,1,0,0,
    0,1,1,0,0,
    0,0,1,0,0,
    0,0,1,0,0,
    0,1,1,1,0
};

const bool numero_2[NUM_PIXELS] = {
    0,1,1,1,0,
    1,0,0,0,1,
    0,0,1,1,0,
    0,1,0,0,0,
    1,1,1,1,1
};

const bool numero_3[NUM_PIXELS] = {
    0,1,1,1,0,
    1,0,0,0,1,
    0,0,1,1,0,
    1,0,0,0,1,
    0,1,1,1,0
};

static inline uint32_t rgb_para_uint32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

void inicializar_matriz_led() {
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, PINO_WS2812, 800000, RGBW_ATIVO);
}

void enviar_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

void mostrar_numero_vidas(int vidas) {
    uint32_t cor = rgb_para_uint32(0, 50, 0); // Cor verde
    const bool* padrao;
    
    // Seleciona o padrão baseado no número de vidas
    switch(vidas) {
        case 3: padrao = numero_3; break;
        case 2: padrao = numero_2; break;
        case 1: padrao = numero_1; break;
        case 0: 
        default: padrao = numero_0; break;
    }
    
    // Exibe o número na matriz LED
    for (int i = 0; i < NUM_PIXELS; i++) {
        enviar_pixel(padrao[i] ? cor : 0);
    }
}

void desligar_matriz() {
    for (int i = 0; i < NUM_PIXELS; i++) {
        enviar_pixel(0);
    }
}