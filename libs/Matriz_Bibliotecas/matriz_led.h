#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

#define PINO_WS2812 7
#define NUM_PIXELS 25
#define RGBW_ATIVO false

// Números para exibir vidas (0-3)
extern const bool numero_0[NUM_PIXELS];
extern const bool numero_1[NUM_PIXELS];
extern const bool numero_2[NUM_PIXELS];
extern const bool numero_3[NUM_PIXELS];

void inicializar_matriz_led();
void enviar_pixel(uint32_t pixel_grb);
void mostrar_numero_vidas(int vidas);
void desligar_matriz();

#endif // MATRIZ_LED_H