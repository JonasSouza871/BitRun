#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "Display_Bibliotecas/ssd1306.h"
#include "matriz_led.h"

// ─── Definições de Hardware ──────────────────────────────────────────────
#define PINO_JOYSTICK_X        27  // Pino ADC para eixo X do joystick
#define PINO_JOYSTICK_Y        26  // Pino ADC para eixo Y do joystick
#define PINO_BOTAO             6   // Botão B para iniciar/reiniciar
#define PINO_BOTAO_A           5   // Botão A para pausar/despausar
#define BUZZER_A               21  // Buzzer A para coletar pixel
#define BUZZER_B               10  // Buzzer B para game over

// ─── Definição dos LEDs de status ──────────────────────────────────────────
#define LED_VERDE              11  // LED verde indica jogo funcionando
#define LED_AZUL               12  // LED azul indica jogo pausado
#define LED_VERMELHO           13  // LED vermelho indica game over

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
volatile bool jogo_pausado = false;
static uint32_t ultimo_pulso_ms = 0;
static uint32_t ultimo_pulso_A_ms = 0;
static uint32_t quadro_splash = 0;

ssd1306_t display;
int pos_jogador_x, pos_jogador_y;
int pos_pixel_x, pos_pixel_y;
int pontuacao = 0;
int vidas = MAX_VIDAS;
bool fim_de_jogo = false;
int centro_x = 2048, centro_y = 2048;  // Médias do joystick após calibração

// ─── Funções para controle dos LEDs ───────────────────────────────────────
void inicializar_leds() {
    // Inicializa os três LEDs como saída
    gpio_init(LED_VERDE);
    gpio_init(LED_AZUL);
    gpio_init(LED_VERMELHO);

    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    // Começa com todos os LEDs desligados
    gpio_put(LED_VERDE, 0);
    gpio_put(LED_AZUL, 0);
    gpio_put(LED_VERMELHO, 0);
}

void atualizar_leds() {
    // Atualiza os LEDs com base no estado atual do jogo
    if (fim_de_jogo) {
        // Game over - LED vermelho ligado, outros desligados
        gpio_put(LED_VERMELHO, 1);
        gpio_put(LED_VERDE, 0);
        gpio_put(LED_AZUL, 0);
    } else if (jogo_pausado) {
        // Jogo pausado - LED azul ligado, outros desligados
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 0);
        gpio_put(LED_AZUL, 1);
    } else if (jogo_iniciado) {
        // Jogo em andamento - LED verde ligado, outros desligados
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 1);
        gpio_put(LED_AZUL, 0);
    } else {
        // Tela inicial ou estado indefinido - todos desligados
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 0);
        gpio_put(LED_AZUL, 0);
    }
}

// ─── Função de Leitura ADC ─────────────────────────────────────────────────
static inline int ler_adc(int canal) {
    adc_select_input(canal);
    return adc_read();
}

// Protótipos de funções para callbacks
void callback_botao_B(uint gpio, uint32_t event);
void callback_botao_A(uint gpio, uint32_t event);

// ─── Trata o botão B com debounce ─────────────────────────────────────────
void callback_botao_B(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_ms < 200) return;
    ultimo_pulso_ms = agora_ms;

    if (fim_de_jogo) {
        // Reinicia o jogo após Game Over
        fim_de_jogo = false;
        jogo_iniciado = true;  // Garante que o jogo reinicie
        atualizar_leds();  // Atualiza os LEDs quando o estado muda
    } else if (!jogo_iniciado) {
        jogo_iniciado = true;
        atualizar_leds();  // Atualiza os LEDs ao iniciar o jogo
    }
}

// ─── Trata o botão A (Pausa) com debounce ───────────────────────────────────
void callback_botao_A(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_A_ms < 200) return;  // Debounce de 200ms
    ultimo_pulso_A_ms = agora_ms;

    // Só alterna a pausa quando o jogo está efetivamente em execução
    if (jogo_iniciado && !fim_de_jogo) {
        jogo_pausado = !jogo_pausado;  // Alterna entre pausado e despausado
        atualizar_leds();  // Atualiza os LEDs quando muda o estado de pausa
    }
}

void init_botoes() {
    // Inicializa botão B (Start/Restart)
    gpio_init(PINO_BOTAO);
    gpio_set_dir(PINO_BOTAO, GPIO_IN);
    gpio_pull_up(PINO_BOTAO);
    gpio_set_irq_enabled_with_callback(
        PINO_BOTAO,
        GPIO_IRQ_EDGE_FALL,
        true,
        callback_botao_B
    );

    // Inicializa botão A (Pause)
    gpio_init(PINO_BOTAO_A);
    gpio_set_dir(PINO_BOTAO_A, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_A);
    gpio_set_irq_enabled_with_callback(
        PINO_BOTAO_A,
        GPIO_IRQ_EDGE_FALL,
        true,
        callback_botao_A
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

// ─── Tela de pausa ────────────────────────────────────────────────────────
void tela_pausa() {
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "JOGO PAUSADO", (LARGURA_TELA-12*6)/2, 20, false);
    ssd1306_draw_string(&display, "A Continuar", (LARGURA_TELA-12*6)/2, ALTURA_TELA-20, false);
    ssd1306_send_data(&display);
    desenhar_vidas();
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

// ─── Funções para controle dos buzzers ───────────────────────────────────────
void inicializar_buzzers() {
    // Inicializa os pinos dos buzzers como saída
    gpio_init(BUZZER_A);
    gpio_init(BUZZER_B);
    gpio_set_dir(BUZZER_A, GPIO_OUT);
    gpio_set_dir(BUZZER_B, GPIO_OUT);
    gpio_put(BUZZER_A, 0);
    gpio_put(BUZZER_B, 0);
}

void tocar_som_pixel() {
    // Toca um som curto e agudo no buzzer A
    for (int i = 0; i < 100; i++) {
        gpio_put(BUZZER_A, 1);
        sleep_us(500);
        gpio_put(BUZZER_A, 0);
        sleep_us(500);
    }
}

void tocar_som_game_over() {
    // Toca um som longo e grave no buzzer B
    for (int i = 0; i < 500; i++) {
        gpio_put(BUZZER_B, 1);
        sleep_us(1000);
        gpio_put(BUZZER_B, 0);
        sleep_us(1000);
    }
}

// ─── Função para imprimir o estado do jogo no monitor serial ────────────────
void imprimir_estado_jogo() {
    int raw_x = ler_adc(1);
    int raw_y = ler_adc(0);
    const char* game_state = jogo_pausado ? "Pausado" : (fim_de_jogo ? "Game Over" : "Jogando");
    printf("Joystick X: %d, Joystick Y: %d, Posição Jogador: (%d, %d), Estado: %s, Pontuação: %d, Vidas: %d\n",
           raw_x, raw_y, pos_jogador_x, pos_jogador_y, game_state, pontuacao, vidas);
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
    jogo_pausado = false;  // Garante que o jogo começa despausado

    // Atualizar LEDs para o estado de jogo em andamento
    atualizar_leds();

    // Define a região onde os pixels aleatórios não podem aparecer
    int area_pontos_x = 2;
    int area_pontos_y = 2;
    int area_pontos_w = 50;
    int area_pontos_h = 10;

    pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
    pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);

    // Verifica se o pixel aleatório está na área de "Pontos" e reposiciona se necessário
    while (colisao(pos_pixel_x, pos_pixel_y, TAM_PIXEL, TAM_PIXEL, area_pontos_x, area_pontos_y, area_pontos_w, area_pontos_h)) {
        pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
        pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);
    }

    uint32_t tempo_imune = 0;
    const uint32_t dur_imune = 1500;
    bool pisca_jogador = false;
    uint32_t ultimo_impressao_ms = 0;

    while (!fim_de_jogo) {
        uint32_t agora = to_ms_since_boot(get_absolute_time());

        // Verifica se o jogo está pausado e atualiza LEDs se necessário
        if (jogo_pausado) {
            tela_pausa();
            sleep_ms(50);
            continue;  // Pula o resto do loop enquanto pausado
        }

        // Imprime o estado do jogo a cada segundo
        if (agora - ultimo_impressao_ms >= 1000) {
            imprimir_estado_jogo();
            ultimo_impressao_ms = agora;
        }

        int raw_x = ler_adc(1);
        int raw_y = ler_adc(0);

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
                // Atualiza LEDs para game over (vermelho)
                atualizar_leds();
                // Toca o som de game over
                tocar_som_game_over();
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

            // Verifica se o novo pixel aleatório está na área de "Pontos" e reposiciona se necessário
            while (colisao(pos_pixel_x, pos_pixel_y, TAM_PIXEL, TAM_PIXEL, area_pontos_x, area_pontos_y, area_pontos_w, area_pontos_h)) {
                pos_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2*BORDAS - TAM_PIXEL);
                pos_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2*BORDAS - TAM_PIXEL);
            }

            // Toca o som de coleta de pixel
            tocar_som_pixel();
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

        // Verifica manualmente o botão B se a interrupção não estiver funcionando
        if (gpio_get(PINO_BOTAO) == 0) { // Se o botão estiver pressionado (ativo em baixo)
            uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
            if (agora_ms - ultimo_pulso_ms > 200) {
                ultimo_pulso_ms = agora_ms;
                fim_de_jogo = false;
                jogo_iniciado = true;  // Garante que o jogo reinicie
                // Atualiza LEDs para o novo estado
                atualizar_leds();
            }
        }
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

    // Inicializa os LEDs de status
    inicializar_leds();

    // Inicializa os buzzers
    inicializar_buzzers();

    // Inicializa ambos os botões com um único sistema de interrupção
    init_botoes();

    // Loop principal para tela inicial
    while (!jogo_iniciado) {
        tela_inicial();
        mostrar_numero_vidas(MAX_VIDAS);
        atualizar_leds();  // Garante que os LEDs estão no estado correto na tela inicial

        // Verifica manualmente o botão B se a interrupção não estiver funcionando
        if (gpio_get(PINO_BOTAO) == 0) { // Se o botão estiver pressionado (ativo em baixo)
            uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
            if (agora_ms - ultimo_pulso_ms > 200) {
                ultimo_pulso_ms = agora_ms;
                jogo_iniciado = true;
                // Atualiza LEDs para o novo estado
                atualizar_leds();
            }
        }

        sleep_ms(30);
    }

    while (1) iniciar_jogo();
    return 0;
}
