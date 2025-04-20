#include "matriz_led.h"

// Definições dos padrões de seta para cima e para baixo
const bool seta_cima[NUM_PIXELS] = {
    0,0,1,0,0,
    0,1,1,1,0,
    1,0,1,0,1,
    0,0,1,0,0,
    0,0,1,0,0
};

const bool seta_baixo[NUM_PIXELS] = {
    0,0,1,0,0,
    0,0,1,0,0,
    1,0,1,0,1,
    0,1,1,1,0,
    0,0,1,0,0
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

void mostrar_seta(bool direcao_cima) {
    const bool* padrao = direcao_cima ? seta_baixo : seta_cima;
    uint32_t cor = rgb_para_uint32(0, 50, 0);
    for (int i = 0; i < NUM_PIXELS; i++) {
        enviar_pixel(padrao[i] ? cor : 0);
    }
}

void desligar_matriz() {
    for (int i = 0; i < NUM_PIXELS; i++) {
        enviar_pixel(0);
    }
}
