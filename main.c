#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "Display_Bibliotecas/ssd1306.h"
#include "matriz_led.h"

// ─── Definições de Hardware ──────────────────────────────────────────────
#define PINO_JOYSTICK_X        27  // Pino ADC para eixo X do joystick
#define PINO_JOYSTICK_Y        26  // Pino ADC para eixo Y do joystick
#define PINO_BOTAO             6   // Botão B para iniciar/reiniciar

// ─── Configurações do Display OLED ──────────────────────────────────────
#define LARGURA_TELA           128
#define ALTURA_TELA            64
#define ENDERECO_OLED          0x3C
#define I2C_PORT               i2c1
#define I2C_SDA_PIN            14
#define I2C_SCL_PIN            15
#define I2C_FREQUENCIA         400000

// ─── Parâmetros do Jogo ─────────────────────────────────────────────────
#define TAM_JOGADOR            8    // Tamanho do quadrado do jogador em px
#define TAM_PIXEL              4    // Tamanho do "pixel" que o jogador coleta
#define BORDAS                2     // Espessura da borda da área de jogo
#define ZONA_MORTA            200   // Margem sem movimento no joystick (ADC)
#define VELOCIDADE            2     // Velocidade do jogador em px por quadro
#define TEMPO_CALIBRAGEM_MS  2000   // Tempo de calibração inicial em ms
#define MAX_VIDAS             3     // Vidas iniciais

// ─── Variáveis Globais ─────────────────────────────────────────────────
volatile bool jogo_iniciado = false;
static uint32_t ultimo_pulso_ms = 0;
static uint32_t quadro_splash = 0;

ssd1306_t display;
int pos_jogador_x, pos_jogador_y;
int pos_pixel_x, pos_pixel_y;
int pontuacao = 0;
int vidas = MAX_VIDAS;
bool fim_de_jogo = false;
int centro_x = 2048, centro_y = 2048;  // Médias do joystick após calibração

// ─── Função de Leitura ADC ─────────────────────────────────────────────────
static inline int ler_adc(int canal) {
    adc_select_input(canal);
    return adc_read();
}

// ─── Trata o botão B com debounce ─────────────────────────────────────────
void callback_botao(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_ms < 200) return;
    ultimo_pulso_ms = agora_ms;

    if (fim_de_jogo) {
        // Reinicia o jogo após Game Over
        fim_de_jogo = false;
        vidas = MAX_VIDAS;
        pontuacao = 0;
        pos_jogador_x = (LARGURA_TELA - TAM_JOGADOR) / 2;
        pos_jogador_y = (ALTURA_TELA - TAM_JOGADOR) / 2;
        pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
        pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);
    } else if (!jogo_iniciado) {
        jogo_iniciado = true;
    }
}

void init_botao() {
    gpio_init(PINO_BOTAO);
    gpio_set_dir(PINO_BOTAO, GPIO_IN);
    gpio_pull_up(PINO_BOTAO);
    gpio_set_irq_enabled_with_callback(
        PINO_BOTAO,
        GPIO_IRQ_EDGE_FALL,
        true,
        callback_botao
    );
}

// ─── Desenha retângulo preenchido ────────────────────────────────────────
void desenhar_retangulo(ssd1306_t *d, int x, int y, int w, int h) {
    for (int i = x; i < x + w; i++)
        for (int j = y; j < y + h; j++)
            ssd1306_pixel(d, i, j, true);
}

// ─── Desenha somente a borda ─────────────────────────────────────────────
void desenhar_borda(ssd1306_t *d, int x, int y, int w, int h, int esp) {
    for (int i = x; i < x + w; i++) {
        ssd1306_pixel(d, i, y, true);
        ssd1306_pixel(d, i, y + h - 1, true);
    }
    for (int j = y; j < y + h; j++) {
        ssd1306_pixel(d, x, j, true);
        ssd1306_pixel(d, x + w - 1, j, true);
    }
}

// ─── Verifica colisão entre dois retângulos ───────────────────────────────
bool colisao(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// ─── Verifica colisão com as bordas da tela ───────────────────────────────
bool bateu_borda(int x, int y, int w, int h) {
    return x < BORDAS || y < BORDAS ||
           x + w > LARGURA_TELA - BORDAS ||
           y + h > ALTURA_TELA - BORDAS;
}

// ─── Mostra pontuação e vidas no OLED ────────────────────────────────────
void desenhar_pontuacao() {
    char buf[20];
    sprintf(buf, "Pontos: %d", pontuacao);
    ssd1306_draw_string(&display, buf, 2, 2, false);
}

void desenhar_vidas() {
    mostrar_numero_vidas(vidas);
}

// ─── Tela inicial animada ────────────────────────────────────────────────
void tela_inicial() {
    quadro_splash++;
    bool pisca = ((quadro_splash / 30) % 2) == 0;
    int offset = (quadro_splash / 20) % 4;

    ssd1306_fill(&display, false);
    ssd1306_pixel(&display, 5, 5, true);
    ssd1306_pixel(&display, LARGURA_TELA-6, 5, true);
    ssd1306_pixel(&display, 5, ALTURA_TELA-6, true);
    ssd1306_pixel(&display, LARGURA_TELA-6, ALTURA_TELA-6, true);
    for (int i = offset; i < LARGURA_TELA-offset; i++) {
        ssd1306_pixel(&display, i, offset, true);
        ssd1306_pixel(&display, i, ALTURA_TELA-1-offset, true);
    }
    for (int j = offset; j < ALTURA_TELA-offset; j++) {
        ssd1306_pixel(&display, offset, j, true);
        ssd1306_pixel(&display, LARGURA_TELA-1-offset, j, true);
    }
    ssd1306_draw_string(&display, "BitRun", (LARGURA_TELA-6*6)/2, 12, false);
    if (pisca) {
        ssd1306_draw_string(&display, "[B] START", (LARGURA_TELA-7*6)/2, ALTURA_TELA-18, false);
        ssd1306_draw_string(&display, ">", (LARGURA_TELA-7*6)/2 - 8, ALTURA_TELA-18, false);
    }
    ssd1306_send_data(&display);
}

// ─── Tela de Game Over ──────────────────────────────────────────────────
void tela_game_over() {
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "GAME OVER", (LARGURA_TELA-9*6)/2, 16, false);
    char buf[20];
    sprintf(buf, "Pontos: %d", pontuacao);
    ssd1306_draw_string(&display, buf, (LARGURA_TELA-strlen(buf)*6)/2, 32, false);
    ssd1306_draw_string(&display, "[B] Reinicia", (LARGURA_TELA-11*6)/2, ALTURA_TELA-16, false);
    ssd1306_send_data(&display);
}

// ─── Inicia o jogo após START ───────────────────────────────────────────
void iniciar_jogo() {
    uint32_t inicio = to_ms_since_boot(get_absolute_time());
    int soma_x = 0, soma_y = 0, cont = 0;
    while (to_ms_since_boot(get_absolute_time()) - inicio < TEMPO_CALIBRAGEM_MS) {
        soma_x += ler_adc(1);
        soma_y += ler_adc(0);
        cont++;
        sleep_ms(5);
    }
    centro_x = soma_x / cont;
    centro_y = soma_y / cont;

    pos_jogador_x = (LARGURA_TELA - TAM_JOGADOR) / 2;
    pos_jogador_y = (ALTURA_TELA - TAM_JOGADOR) / 2;
    pontuacao = 0;
    vidas = MAX_VIDAS;
    fim_de_jogo = false;
    pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
    pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);

    uint32_t tempo_imune = 0;
    const uint32_t dur_imune = 1500;
    bool pisca_jogador = false;

    while (!fim_de_jogo) {
        int raw_x = ler_adc(1);
        int raw_y = ler_adc(0);
        uint32_t agora = to_ms_since_boot(get_absolute_time());

        // Reseta imunidade após duração
        if (tempo_imune > 0 && agora >= tempo_imune) {
            tempo_imune = 0;
        }

        int dx = 0, dy = 0;
        if (raw_x > centro_x + ZONA_MORTA) dx = VELOCIDADE;
        else if (raw_x < centro_x - ZONA_MORTA) dx = -VELOCIDADE;
        if (raw_y > centro_y + ZONA_MORTA) dy = -VELOCIDADE;
        else if (raw_y < centro_y - ZONA_MORTA) dy = VELOCIDADE;

        int nova_x = pos_jogador_x + dx;
        int nova_y = pos_jogador_y + dy;

        // Colisão com borda (sem imunidade)
        if (tempo_imune == 0 && bateu_borda(nova_x, nova_y, TAM_JOGADOR, TAM_JOGADOR)) {
            vidas--;
            if (vidas <= 0) {
                fim_de_jogo = true;
                desligar_matriz();
                break;
            }
            tempo_imune = agora + dur_imune;
        }

        // Move o jogador
        if (!bateu_borda(nova_x, nova_y, TAM_JOGADOR, TAM_JOGADOR) || tempo_imune > 0) {
            pos_jogador_x = nova_x;
            pos_jogador_y = nova_y;
        }

        // Coleta pixel?
        if (colisao(pos_jogador_x, pos_jogador_y, TAM_JOGADOR, TAM_JOGADOR,
                   pos_pixel_x, pos_pixel_y, TAM_PIXEL, TAM_PIXEL)) {
            pontuacao++;
            pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
            pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);
        }

        ssd1306_fill(&display, false);
        desenhar_borda(&display, 0, 0, LARGURA_TELA, ALTURA_TELA, BORDAS);
        pisca_jogador = (tempo_imune > 0) && (((agora / 150) % 2) == 0);
        if (!pisca_jogador) desenhar_retangulo(&display, pos_jogador_x, pos_jogador_y, TAM_JOGADOR, TAM_JOGADOR);
        desenhar_retangulo(&display, pos_pixel_x, pos_pixel_y, TAM_PIXEL, TAM_PIXEL);
        desenhar_pontuacao();
        desenhar_vidas();
        ssd1306_send_data(&display);

        sleep_ms(30);
    }

    while (fim_de_jogo) {
        tela_game_over();
        mostrar_numero_vidas(0);
        sleep_ms(50);
    }
}

int main() {
    stdio_init_all();
    i2c_init(I2C_PORT, I2C_FREQUENCIA);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    adc_init();
    adc_gpio_init(PINO_JOYSTICK_X);
    adc_gpio_init(PINO_JOYSTICK_Y);
    ssd1306_init(&display, LARGURA_TELA, ALTURA_TELA, false, ENDERECO_OLED, I2C_PORT);
    ssd1306_config(&display);
    inicializar_matriz_led();
    init_botao();
    while (!jogo_iniciado) {
        tela_inicial();
        mostrar_numero_vidas(MAX_VIDAS);
        tight_loop_contents();
        sleep_ms(30);
    }
    while (1) iniciar_jogo();
    return 0;
}
