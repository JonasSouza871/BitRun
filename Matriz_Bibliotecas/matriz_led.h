#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

#define PINO_WS2812 7
#define NUM_PIXELS 25
#define RGBW_ATIVO false

extern const bool seta_cima[NUM_PIXELS];
extern const bool seta_baixo[NUM_PIXELS];

void inicializar_matriz_led();
void enviar_pixel(uint32_t pixel_grb);
void mostrar_seta(bool direcao_cima);
void desligar_matriz();

#endif // MATRIZ_LED_H
