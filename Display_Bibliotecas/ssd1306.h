#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t pages;
    uint8_t address;
    i2c_inst_t *i2c_port;
    uint16_t bufsize;
    uint8_t *ram_buffer;
    uint8_t port_buffer[2];
} ssd1306_t;

// Funções existentes permanecem iguais
void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c);
void ssd1306_config(ssd1306_t *ssd);
void ssd1306_command(ssd1306_t *ssd, uint8_t command);
void ssd1306_send_data(ssd1306_t *ssd);
void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value);
void ssd1306_fill(ssd1306_t *ssd, bool value);
void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill);
void ssd1306_line(ssd1306_t *ssd, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value);
void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value);
void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value);

// Novas funções para suportar números pequenos
void ssd1306_draw_small_number(ssd1306_t *ssd, char c, uint8_t x, uint8_t y);
void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y, bool use_small_numbers);
void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y, bool use_small_numbers);

#endif // SSD1306_H
