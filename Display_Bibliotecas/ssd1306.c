#include "ssd1306.h"
#include "font.h"
#include <stdlib.h>
#include "hardware/i2c.h"

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
    ssd->width = width;
    ssd->height = height;
    ssd->pages = height / 8;
    ssd->address = address;
    ssd->i2c_port = i2c;
    ssd->bufsize = ssd->pages * ssd->width + 1;
    ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
    if (ssd->ram_buffer != NULL) {
        ssd->ram_buffer[0] = 0x40; // Co = 0, D/C = 1
    }
    ssd->port_buffer[0] = 0x80; // Co = 1, D/C = 0
}

void ssd1306_config(ssd1306_t *ssd) {
    ssd1306_command(ssd, 0xAE); // Display off
    ssd1306_command(ssd, 0x20); // Memory mode
    ssd1306_command(ssd, 0x00); // Horizontal addressing mode
    ssd1306_command(ssd, 0x40); // Start line
    ssd1306_command(ssd, 0xA1); // Segment remap
    ssd1306_command(ssd, 0xA8); // Multiplex ratio
    ssd1306_command(ssd, ssd->height - 1);
    ssd1306_command(ssd, 0xC8); // COM output scan direction
    ssd1306_command(ssd, 0xD3); // Display offset
    ssd1306_command(ssd, 0x00);
    ssd1306_command(ssd, 0xDA); // COM pin config
    ssd1306_command(ssd, 0x12);
    ssd1306_command(ssd, 0xD5); // Display clock divide ratio
    ssd1306_command(ssd, 0x80);
    ssd1306_command(ssd, 0xD9); // Pre-charge period
    ssd1306_command(ssd, 0xF1);
    ssd1306_command(ssd, 0xDB); // VCOM deselect level
    ssd1306_command(ssd, 0x30);
    ssd1306_command(ssd, 0x81); // Contrast control
    ssd1306_command(ssd, 0xFF);
    ssd1306_command(ssd, 0xA4); // Entire display on
    ssd1306_command(ssd, 0xA6); // Normal display
    ssd1306_command(ssd, 0x8D); // Charge pump setting
    ssd1306_command(ssd, 0x14);
    ssd1306_command(ssd, 0xAF); // Display on
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command) {
    ssd->port_buffer[1] = command;
    i2c_write_blocking(ssd->i2c_port, ssd->address, ssd->port_buffer, 2, false);
}

void ssd1306_send_data(ssd1306_t *ssd) {
    ssd1306_command(ssd, 0x21); // Column address
    ssd1306_command(ssd, 0);
    ssd1306_command(ssd, ssd->width - 1);
    ssd1306_command(ssd, 0x22); // Page address
    ssd1306_command(ssd, 0);
    ssd1306_command(ssd, ssd->pages - 1);
    i2c_write_blocking(ssd->i2c_port, ssd->address, ssd->ram_buffer, ssd->bufsize, false);
}

void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value) {
    uint16_t index = (y / 8) * ssd->width + x + 1;
    uint8_t pixel = y % 8;
    if (value) {
        ssd->ram_buffer[index] |= (1 << pixel);
    } else {
        ssd->ram_buffer[index] &= ~(1 << pixel);
    }
}

void ssd1306_fill(ssd1306_t *ssd, bool value) {
    for (uint8_t y = 0; y < ssd->height; ++y) {
        for (uint8_t x = 0; x < ssd->width; ++x) {
            ssd1306_pixel(ssd, x, y, value);
        }
    }
}

void ssd1306_draw_small_number(ssd1306_t *ssd, char c, uint8_t x, uint8_t y) {
    if (c >= '0' && c <= '9') {
        // Índice na fonte para o número correspondente
        uint16_t index = ((c - '0') * 5) + (68 * 8); // Baseado na estrutura da fonte

        for (uint8_t i = 0; i < 5; ++i) {
            uint8_t line = font[index + i]; // Linha da matriz 5x5 do número
            for (uint8_t j = 0; j < 5; ++j) {
                // Inverter a ordem dos bits ao desenhar
                if ((line >> (4 - j)) & 0x01) {
                    ssd1306_pixel(ssd, x + j, y + i, true);
                }
            }
        }
    }
}



void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y, bool use_small_numbers) {
    if (use_small_numbers && c >= '0' && c <= '9') {
        ssd1306_draw_small_number(ssd, c, x, y);
        return;
    }

    uint16_t index = 0;
    bool rotate = false; // Flag para rotação

    if (c >= '0' && c <= '9') {
        index = (c - '0' + 1) * 8;
    } else if (c >= 'A' && c <= 'Z') {
        index = (c - 'A' + 11) * 8;
    } else if (c >= 'a' && c <= 'z') {
        index = (c - 'a' + 37) * 8;
    } else if (c == ':') {
        index = 64 * 8; // Índice para ':'
        rotate = true;
    } else if (c == '.') {
        index = 65 * 8; // Índice para '.'
        rotate = true;
    } else if (c == '>') {
        index = 66 * 8; // Índice para '>'
        rotate = true;
    } else if (c == '-') {
        index = 67 * 8; // Índice para '-'
        rotate = true;
    }

    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t line = font[index + i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (rotate) {
                // Rotaciona 90° para símbolos especiais
                ssd1306_pixel(ssd, x + (7 - j), y + i, (line >> j) & 0x01);
            } else {
                // Caracteres normais sem rotação
                ssd1306_pixel(ssd, x + i, y + j, (line >> j) & 0x01);
            }
        }
    }
}

void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y, bool use_small_numbers) {
    while (*str) {
        char c = *str;
        uint8_t char_width = 8; // Padrão para caracteres normais
        
        if (use_small_numbers && c >= '0' && c <= '9') {
            char_width = 5; // Largura reduzida para números pequenos
        }

        // Verifica se o caractere cabe na linha atual
        if (x + char_width > ssd->width) {
            x = 0;
            y += 8; // Próxima linha
            if (y + 8 > ssd->height) break; // Sem espaço vertical
        }

        ssd1306_draw_char(ssd, c, x, y, use_small_numbers);
        x += char_width; // Avança a posição horizontal
        str++;
    }
}

void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill) {
    for (uint8_t x = left; x < left + width; ++x) {
        ssd1306_pixel(ssd, x, top, value);
        ssd1306_pixel(ssd, x, top + height - 1, value);
    }
    for (uint8_t y = top; y < top + height; ++y) {
        ssd1306_pixel(ssd, left, y, value);
        ssd1306_pixel(ssd, left + width - 1, y, value);
    }
    if (fill) {
        for (uint8_t x = left + 1; x < left + width - 1; ++x) {
            for (uint8_t y = top + 1; y < top + height - 1; ++y) {
                ssd1306_pixel(ssd, x, y, value);
            }
        }
    }
}

void ssd1306_line(ssd1306_t *ssd, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        ssd1306_pixel(ssd, x0, y0, value);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value) {
    for (uint8_t x = x0; x <= x1; ++x) {
        ssd1306_pixel(ssd, x, y, value);
    }
}

void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value) {
    for (uint8_t y = y0; y <= y1; ++y) {
        ssd1306_pixel(ssd, x, y, value);
    }
}
