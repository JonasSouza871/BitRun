#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "libs\Display_Bibliotecas\ssd1306.h"
#include "libs\Matriz_Bibliotecas\matriz_led.h"

// ─── Definições de Hardware ──────────────────────────────────────────────
#define PINO_JOYSTICK_X        27  // Pino ADC para eixo X do joystick
#define PINO_JOYSTICK_Y        26  // Pino ADC para eixo Y do joystick
#define PINO_BOTAO             6   // Botão B para iniciar/reiniciar
#define PINO_BOTAO_A           5   // Botão A para pausar/despausar
#define PINO_BOTAO_JOYSTICK    22  // Botão do joystick para pausar/despausar
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
#define TAMANHO_JOGADOR        8    // Tamanho do quadrado do jogador em px
#define TAMANHO_PIXEL          4    // Tamanho do "pixel" que o jogador coleta
#define BORDAS                 2    // Espessura da borda da área de jogo
#define ZONA_MORTA            200   // Margem sem movimento no joystick (ADC)
#define VELOCIDADE            2     // Velocidade do jogador em px por quadro
#define TEMPO_CALIBRAGEM_MS   2000  // Tempo de calibração inicial em ms
#define MAX_VIDAS             3     // Vidas iniciais

// ─── Variáveis Globais ─────────────────────────────────────────────────
volatile bool jogo_iniciado = false;
volatile bool jogo_pausado = false;
static uint32_t ultimo_pulso_ms = 0;
static uint32_t ultimo_pulso_A_ms = 0;
static uint32_t ultimo_pulso_joystick_ms = 0; // Adicionado para o botão do joystick
static uint32_t quadro_splash = 0;

ssd1306_t display;
int posicao_jogador_x, posicao_jogador_y;
int posicao_pixel_x, posicao_pixel_y;
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
        gpio_put(LED_VERMELHO, 1);
        gpio_put(LED_VERDE, 0);
        gpio_put(LED_AZUL, 0);
    } else if (jogo_pausado) {
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 0);
        gpio_put(LED_AZUL, 1);
    } else if (jogo_iniciado) {
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 1);
        gpio_put(LED_AZUL, 0);
    } else {
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
void callback_botao_joystick(uint gpio, uint32_t event); // Adicionado para o botão do joystick

// ─── Trata o botão B com debounce ─────────────────────────────────────────
void callback_botao_B(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_ms < 200) return;
    ultimo_pulso_ms = agora_ms;

    if (fim_de_jogo) {
        // Reinicia o jogo após Game Over
        fim_de_jogo = false;
        jogo_iniciado = true;
        atualizar_leds();
    } else if (!jogo_iniciado) {
        jogo_iniciado = true;
        atualizar_leds();
    }
}

// ─── Trata o botão A (Pausa) com debounce ───────────────────────────────────
void callback_botao_A(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_A_ms < 200) return;
    ultimo_pulso_A_ms = agora_ms;

    if (jogo_iniciado && !fim_de_jogo) {
        jogo_pausado = !jogo_pausado;
        atualizar_leds();
    }
}

// ─── Trata o botão do joystick (Pausa) com debounce ───────────────────────────────────
void callback_botao_joystick(uint gpio, uint32_t event) {
    uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
    if (agora_ms - ultimo_pulso_joystick_ms < 200) return;
    ultimo_pulso_joystick_ms = agora_ms;

    if (jogo_iniciado && !fim_de_jogo) {
        jogo_pausado = !jogo_pausado;
        atualizar_leds();
    }
}

void inicializar_botoes() {
    // Inicializa botão B (Start/Restart)
    gpio_init(PINO_BOTAO);
    gpio_set_dir(PINO_BOTAO, GPIO_IN);
    gpio_pull_up(PINO_BOTAO);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO, GPIO_IRQ_EDGE_FALL, true, callback_botao_B);

    // Inicializa botão A (Pause)
    gpio_init(PINO_BOTAO_A);
    gpio_set_dir(PINO_BOTAO_A, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_A);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_A, GPIO_IRQ_EDGE_FALL, true, callback_botao_A);

    // Inicializa botão do joystick (Pause)
    gpio_init(PINO_BOTAO_JOYSTICK);
    gpio_set_dir(PINO_BOTAO_JOYSTICK, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_JOYSTICK);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, callback_botao_joystick);
}

// ─── Desenha retângulo preenchido ────────────────────────────────────────
void desenhar_retangulo(ssd1306_t *display, int x, int y, int largura, int altura) {
    for (int i = x; i < x + largura; i++) {
        for (int j = y; j < y + altura; j++) {
            ssd1306_pixel(display, i, j, true);
        }
    }
}

// ─── Desenha somente a borda ─────────────────────────────────────────────
void desenhar_borda(ssd1306_t *display, int x, int y, int largura, int altura, int espessura) {
    for (int i = x; i < x + largura; i++) {
        ssd1306_pixel(display, i, y, true);
        ssd1306_pixel(display, i, y + altura - 1, true);
    }
    for (int j = y; j < y + altura; j++) {
        ssd1306_pixel(display, x, j, true);
        ssd1306_pixel(display, x + largura - 1, j, true);
    }
}

// ─── Verifica colisão entre dois retângulos ───────────────────────────────
bool verificar_colisao(int ax, int ay, int largura_a, int altura_a, int bx, int by, int largura_b, int altura_b) {
    // Verifica se há sobreposição nos eixos X e Y
    return ax < bx + largura_b && ax + largura_a > bx && ay < by + altura_b && ay + altura_a > by;
}

// ─── Verifica colisão com as bordas da tela ───────────────────────────────
bool verificar_colisao_borda(int x, int y, int largura, int altura) {
    // Verifica se o retângulo ultrapassa as bordas da tela
    return x < BORDAS || y < BORDAS || x + largura > LARGURA_TELA - BORDAS || y + altura > ALTURA_TELA - BORDAS;
}

// ─── Mostra pontuação e vidas no OLED ────────────────────────────────────
void desenhar_pontuacao() {
    char buffer[20];
    sprintf(buffer, "Pontos: %d", pontuacao);
    ssd1306_draw_string(&display, buffer, 2, 2, false);
}

void desenhar_vidas() {
    mostrar_numero_vidas(vidas);
}

// ─── Tela inicial animada ────────────────────────────────────────────────
void tela_inicial() {
    quadro_splash++;
    bool pisca = ((quadro_splash / 30) % 2) == 0;
    int deslocamento = (quadro_splash / 20) % 4;

    ssd1306_fill(&display, false);
    ssd1306_pixel(&display, 5, 5, true);
    ssd1306_pixel(&display, LARGURA_TELA - 6, 5, true);
    ssd1306_pixel(&display, 5, ALTURA_TELA - 6, true);
    ssd1306_pixel(&display, LARGURA_TELA - 6, ALTURA_TELA - 6, true);

    for (int i = deslocamento; i < LARGURA_TELA - deslocamento; i++) {
        ssd1306_pixel(&display, i, deslocamento, true);
        ssd1306_pixel(&display, i, ALTURA_TELA - 1 - deslocamento, true);
    }
    for (int j = deslocamento; j < ALTURA_TELA - deslocamento; j++) {
        ssd1306_pixel(&display, deslocamento, j, true);
        ssd1306_pixel(&display, LARGURA_TELA - 1 - deslocamento, j, true);
    }

    ssd1306_draw_string(&display, "BitRun", (LARGURA_TELA - 6 * 6) / 2, 12, false);
    if (pisca) {
        ssd1306_draw_string(&display, "[B] START", (LARGURA_TELA - 7 * 6) / 2, ALTURA_TELA - 18, false);
        ssd1306_draw_string(&display, ">", (LARGURA_TELA - 7 * 6) / 2 - 8, ALTURA_TELA - 18, false);
    }
    ssd1306_send_data(&display);
}

// ─── Tela de pausa ────────────────────────────────────────────────────────
void tela_pausa() {
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "JOGO PAUSADO", (LARGURA_TELA - 12 * 6) / 2, 20, false);
    ssd1306_draw_string(&display, "A Continuar", (LARGURA_TELA - 12 * 6) / 2, ALTURA_TELA - 20, false);
    ssd1306_send_data(&display);
    desenhar_vidas();
}

// ─── Tela de Game Over ──────────────────────────────────────────────────
void tela_game_over() {
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "GAME OVER", (LARGURA_TELA - 9 * 6) / 2, 16, false);
    char buffer[20];
    sprintf(buffer, "Pontos: %d", pontuacao);
    ssd1306_draw_string(&display, buffer, (LARGURA_TELA - strlen(buffer) * 6) / 2, 32, false);
    ssd1306_draw_string(&display, "[B] Reinicia", (LARGURA_TELA - 11 * 6) / 2, ALTURA_TELA - 16, false);
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
    int valor_x = ler_adc(1);
    int valor_y = ler_adc(0);
    const char* estado_jogo = jogo_pausado ? "Pausado" : (fim_de_jogo ? "Game Over" : "Jogando");
    printf("Joystick X: %d, Joystick Y: %d, Posição Jogador: (%d, %d), Estado: %s, Pontuação: %d, Vidas: %d\n",
           valor_x, valor_y, posicao_jogador_x, posicao_jogador_y, estado_jogo, pontuacao, vidas);
}

// ─── Inicia o jogo após START ───────────────────────────────────────────
void iniciar_jogo() {
    uint32_t inicio = to_ms_since_boot(get_absolute_time());
    int soma_x = 0, soma_y = 0, contador = 0;

    // Calibração inicial do joystick
    while (to_ms_since_boot(get_absolute_time()) - inicio < TEMPO_CALIBRAGEM_MS) {
        soma_x += ler_adc(1);
        soma_y += ler_adc(0);
        contador++;
        sleep_ms(5);
    }

    // Calcula a média dos valores lidos para centralizar o joystick
    centro_x = soma_x / contador;
    centro_y = soma_y / contador;

    posicao_jogador_x = (LARGURA_TELA - TAMANHO_JOGADOR) / 2;
    posicao_jogador_y = (ALTURA_TELA - TAMANHO_JOGADOR) / 2;
    pontuacao = 0;
    vidas = MAX_VIDAS;
    fim_de_jogo = false;
    jogo_pausado = false;

    atualizar_leds();

    int area_pontos_x = 2;
    int area_pontos_y = 2;
    int area_pontos_largura = 50;
    int area_pontos_altura = 10;

    // Posiciona o pixel aleatoriamente, evitando a área de pontos
    posicao_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
    posicao_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);

    while (verificar_colisao(posicao_pixel_x, posicao_pixel_y, TAMANHO_PIXEL, TAMANHO_PIXEL, area_pontos_x, area_pontos_y, area_pontos_largura, area_pontos_altura)) {
        posicao_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
        posicao_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
    }

    uint32_t tempo_imune = 0;
    const uint32_t duracao_imune = 1500;
    bool pisca_jogador = false;
    uint32_t ultimo_impressao_ms = 0;

    while (!fim_de_jogo) {
        uint32_t agora = to_ms_since_boot(get_absolute_time());

        if (jogo_pausado) {
            tela_pausa();
            sleep_ms(50);
            continue;
        }

        if (agora - ultimo_impressao_ms >= 1000) {
            imprimir_estado_jogo();
            ultimo_impressao_ms = agora;
        }

        int valor_x = ler_adc(1);
        int valor_y = ler_adc(0);

        // Reseta imunidade após duração
        if (tempo_imune > 0 && agora >= tempo_imune) {
            tempo_imune = 0;
        }

        int dx = 0, dy = 0;
        // Movimento no eixo X
        if (valor_x > centro_x + ZONA_MORTA) dx = VELOCIDADE;
        else if (valor_x < centro_x - ZONA_MORTA) dx = -VELOCIDADE;
        // Movimento no eixo Y
        if (valor_y > centro_y + ZONA_MORTA) dy = -VELOCIDADE;
        else if (valor_y < centro_y - ZONA_MORTA) dy = VELOCIDADE;

        int nova_posicao_x = posicao_jogador_x + dx;
        int nova_posicao_y = posicao_jogador_y + dy;

        // Verifica colisão com borda (sem imunidade)
        if (tempo_imune == 0 && verificar_colisao_borda(nova_posicao_x, nova_posicao_y, TAMANHO_JOGADOR, TAMANHO_JOGADOR)) {
            vidas--;
            if (vidas <= 0) {
                fim_de_jogo = true;
                desligar_matriz();
                atualizar_leds();
                tocar_som_game_over();
                break;
            }
            tempo_imune = agora + duracao_imune;
        }

        // Move o jogador se não houver colisão ou se estiver imune
        if (!verificar_colisao_borda(nova_posicao_x, nova_posicao_y, TAMANHO_JOGADOR, TAMANHO_JOGADOR) || tempo_imune > 0) {
            posicao_jogador_x = nova_posicao_x;
            posicao_jogador_y = nova_posicao_y;
        }

        // Verifica se o jogador coletou o pixel
        if (verificar_colisao(posicao_jogador_x, posicao_jogador_y, TAMANHO_JOGADOR, TAMANHO_JOGADOR,
                              posicao_pixel_x, posicao_pixel_y, TAMANHO_PIXEL, TAMANHO_PIXEL)) {
            pontuacao++;
            // Reposiciona o pixel aleatoriamente, evitando a área de pontos
            posicao_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
            posicao_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);

            while (verificar_colisao(posicao_pixel_x, posicao_pixel_y, TAMANHO_PIXEL, TAMANHO_PIXEL, area_pontos_x, area_pontos_y, area_pontos_largura, area_pontos_altura)) {
                posicao_pixel_x = BORDAS + rand() % (LARGURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
                posicao_pixel_y = BORDAS + rand() % (ALTURA_TELA - 2 * BORDAS - TAMANHO_PIXEL);
            }

            tocar_som_pixel();
        }

        ssd1306_fill(&display, false);
        desenhar_borda(&display, 0, 0, LARGURA_TELA, ALTURA_TELA, BORDAS);
        pisca_jogador = (tempo_imune > 0) && (((agora / 150) % 2) == 0);
        if (!pisca_jogador) desenhar_retangulo(&display, posicao_jogador_x, posicao_jogador_y, TAMANHO_JOGADOR, TAMANHO_JOGADOR);
        desenhar_retangulo(&display, posicao_pixel_x, posicao_pixel_y, TAMANHO_PIXEL, TAMANHO_PIXEL);
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
        if (gpio_get(PINO_BOTAO) == 0) {
            uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
            if (agora_ms - ultimo_pulso_ms > 200) {
                ultimo_pulso_ms = agora_ms;
                fim_de_jogo = false;
                jogo_iniciado = true;
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

    inicializar_leds();
    inicializar_buzzers();
    inicializar_botoes();

    while (!jogo_iniciado) {
        tela_inicial();
        mostrar_numero_vidas(MAX_VIDAS);
        atualizar_leds();

        // Verifica manualmente o botão B se a interrupção não estiver funcionando
        if (gpio_get(PINO_BOTAO) == 0) {
            uint32_t agora_ms = to_ms_since_boot(get_absolute_time());
            if (agora_ms - ultimo_pulso_ms > 200) {
                ultimo_pulso_ms = agora_ms;
                jogo_iniciado = true;
                atualizar_leds();
            }
        }

        sleep_ms(30);
    }

    while (1) iniciar_jogo();
    return 0;
}
