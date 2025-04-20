#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "Display_Bibliotecas/ssd1306.h"

/* ─── Definições de Hardware ────────────────────────────────────────────── */
#define PINO_JOYSTICK_X    27
#define PINO_JOYSTICK_Y    26
#define PINO_BOTAO_B       6
#define DEBOUNCE_MS        200

#define I2C_PORT           i2c1
#define I2C_SDA            14
#define I2C_SCL            15
#define I2C_FREQ           400000

#define LARGURA_TELA       128
#define ALTURA_TELA        64
#define ENDERECO_DISPLAY   0x3C

#define TAM_JOGADOR        8
#define TAM_PIXEL          4
#define ZONA_MORTA_RAW     500
#define MAX_VEL            2
#define CALIBRAGEM_MS      2000

/* ─── Variáveis globais ─────────────────────────────────────────────────── */
volatile bool jogoIniciado = false;
static uint32_t ultimoPulso = 0;
static uint32_t frameSplash = 0;

ssd1306_t display;
int jogadorX, jogadorY;
int pixelX, pixelY;
int pontuacao = 0;
int centroXRaw = 2048, centroYRaw = 2048;

/* ─── ISR do botão B (com debounce) ─────────────────────────────────────── */
void tratamentoBotaoB(uint gpio, uint32_t event) {
    uint32_t agora = to_ms_since_boot(get_absolute_time());
    if (agora - ultimoPulso > DEBOUNCE_MS) {
        ultimoPulso = agora;
        jogoIniciado = true;
    }
}

/* ─── Inicializa hardware do botão B ───────────────────────────────────── */
void inicializarBotaoB() {
    gpio_init(PINO_BOTAO_B);
    gpio_set_dir(PINO_BOTAO_B, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_B);
    gpio_set_irq_enabled_with_callback(
        PINO_BOTAO_B,
        GPIO_IRQ_EDGE_FALL,
        true,
        tratamentoBotaoB
    );
}

/* ─── Funções utilitárias ───────────────────────────────────────────────── */
static inline int16_t lerADC(int canal) {
    adc_select_input(canal);
    return adc_read();
}

int calcularVelocidade(int raw, int centro) {
    int delta = raw - centro;
    if (delta > -ZONA_MORTA_RAW && delta < ZONA_MORTA_RAW) return 0;
    int vel = (abs(delta) - ZONA_MORTA_RAW) * MAX_VEL / 1024;
    return vel > MAX_VEL ? MAX_VEL : vel;
}

void desenharRect(ssd1306_t *d, int x, int y, int w, int h) {
    for (int i = x; i < x + w; i++)
        for (int j = y; j < y + h; j++)
            if (i >= 0 && i < LARGURA_TELA && j >= 0 && j < ALTURA_TELA)
                ssd1306_pixel(d, i, j, true);
}

bool colisao(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

void reposicionarPixel() {
    pixelX = rand() % (LARGURA_TELA - TAM_PIXEL);
    pixelY = rand() % (ALTURA_TELA - TAM_PIXEL);
}

void desenharPontuacao(int p) {
    char buf[16];
    sprintf(buf, "Pontos:%d", p);
    ssd1306_draw_string(&display, buf, 0, 0, false);
}

/* ─── Splash screen animada ─────────────────────────────────────────────── */
void exibirTelaInicial() {
    frameSplash++;
    bool pisca = ((frameSplash / 30) & 1) == 0;     // pisca ~0.5s
    int offset = (frameSplash / 20) % 4;            // anima borda

    ssd1306_fill(&display, false);

    // estrelas nos cantos
    ssd1306_pixel(&display, 5, 5, true);
    ssd1306_pixel(&display, LARGURA_TELA-6, 5, true);
    ssd1306_pixel(&display, 5, ALTURA_TELA-6, true);
    ssd1306_pixel(&display, LARGURA_TELA-6, ALTURA_TELA-6, true);

    // borda animada
    for (int x = offset; x < LARGURA_TELA-offset; x++) {
        ssd1306_pixel(&display, x, offset, true);
        ssd1306_pixel(&display, x, ALTURA_TELA-1-offset, true);
    }
    for (int y = offset; y < ALTURA_TELA-offset; y++) {
        ssd1306_pixel(&display, offset, y, true);
        ssd1306_pixel(&display, LARGURA_TELA-1-offset, y, true);
    }

    // título centralizado
    const char *titulo = "BitRun";
    int tw = strlen(titulo) * 6;
    int tx = (LARGURA_TELA - tw) / 2;
    int ty = 12;
    ssd1306_draw_string(&display, titulo, tx, ty, false);

    // linha única abaixo do título
    int uy = ty + 10;
    for (int x = tx; x < tx + tw; x++)
        ssd1306_pixel(&display, x, uy, true);

    // subtítulo piscante
    if (pisca) {
        const char *sub = "O Melhor Jogo!";
        int sw = strlen(sub) * 6;
        int sx = (LARGURA_TELA - sw) / 2;
        int sy = ty + 18;
        ssd1306_draw_string(&display, sub, sx, sy, true);
    }

    // prompt piscante com seta
    if (pisca) {
        const char *p = "[B] START";
        int pw = strlen(p) * 6;
        int px = (LARGURA_TELA - pw) / 2;
        int py = ALTURA_TELA - 18;
        ssd1306_draw_string(&display, p, px, py, false);
        ssd1306_draw_string(&display, ">", px - 8, py, false);
    }

    ssd1306_send_data(&display);
}

/* ─── Lógica principal do jogo ──────────────────────────────────────────── */
void iniciarJogo() {
    // calibração do joystick
    uint32_t t0 = to_ms_since_boot(get_absolute_time()), cont = 0;
    uint32_t sx = 0, sy = 0;
    while (to_ms_since_boot(get_absolute_time()) - t0 < CALIBRAGEM_MS) {
        sx += lerADC(1);
        sy += lerADC(0);
        cont++;
        sleep_ms(5);
    }
    centroXRaw = sx / cont;
    centroYRaw = sy / cont;

    jogadorX = (LARGURA_TELA - TAM_JOGADOR) / 2;
    jogadorY = (ALTURA_TELA - TAM_JOGADOR) / 2;
    pontuacao = 0;
    reposicionarPixel();

    while (true) {
        int rawX = lerADC(1), rawY = lerADC(0);
        int vx = calcularVelocidade(rawX, centroXRaw);
        int vy = calcularVelocidade(rawY, centroYRaw);

        if (rawX < centroXRaw - ZONA_MORTA_RAW) jogadorX -= vx;
        else if (rawX > centroXRaw + ZONA_MORTA_RAW) jogadorX += vx;
        if (rawY < centroYRaw - ZONA_MORTA_RAW) jogadorY += vy;
        else if (rawY > centroYRaw + ZONA_MORTA_RAW) jogadorY -= vy;

        if (jogadorX < 0) jogadorX = 0;
        if (jogadorX + TAM_JOGADOR > LARGURA_TELA) jogadorX = LARGURA_TELA - TAM_JOGADOR;
        if (jogadorY < 0) jogadorY = 0;
        if (jogadorY + TAM_JOGADOR > ALTURA_TELA) jogadorY = ALTURA_TELA - TAM_JOGADOR;

        if (colisao(jogadorX,jogadorY,TAM_JOGADOR,TAM_JOGADOR,
                   pixelX,pixelY,TAM_PIXEL,TAM_PIXEL)) {
            pontuacao++;
            reposicionarPixel();
        }

        ssd1306_fill(&display, false);
        desenharRect(&display, jogadorX, jogadorY, TAM_JOGADOR, TAM_JOGADOR);
        desenharRect(&display, pixelX, pixelY, TAM_PIXEL, TAM_PIXEL);
        desenharPontuacao(pontuacao);
        ssd1306_send_data(&display);

        sleep_ms(30);
    }
}

/* ─── Main ──────────────────────────────────────────────────────────────── */
int main() {
    stdio_init_all();

    // I2C
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // ADC
    adc_init();
    adc_gpio_init(PINO_JOYSTICK_X);
    adc_gpio_init(PINO_JOYSTICK_Y);

    // display
    ssd1306_init(&display, LARGURA_TELA, ALTURA_TELA, false, ENDERECO_DISPLAY, I2C_PORT);
    ssd1306_config(&display);

    // botão B
    inicializarBotaoB();

    // splash animada até o botão ser pressionado
    while (!jogoIniciado) {
        exibirTelaInicial();
        tight_loop_contents();
        sleep_ms(30);
    }

    // inicia o jogo
    iniciarJogo();
    return 0;
}
