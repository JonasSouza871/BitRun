# 🕹️ BitRun

BitRun é um jogo simples de coleta de pixels desenvolvido para a placa Raspberry Pi Pico W. O jogador controla um quadrado usando um joystick analógico em um display OLED SSD1306, com o objetivo de coletar "pixels" que aparecem aleatoriamente na tela. As vidas restantes são exibidas em uma matriz de LEDs WS2812 (5x5), e o estado do jogo (ativo, pausado, game over) é indicado por LEDs de status e feedback sonoro através de buzzers.

---

## 🚀 Funcionalidades

- ✅ Controle do jogador via Joystick Analógico (X/Y).
- ✅ Gameplay de coleta de pixels com pontuação.
- ✅ Display OLED (SSD1306) para visualização do jogo, pontuação e status.
- ✅ Matriz de LEDs WS2812 (5x5) para exibição visual das vidas restantes.
- ✅ Sistema de vidas com feedback visual e sonoro.
- ✅ Estados de Jogo: Tela Inicial, Jogando, Pausado, Fim de Jogo (Game Over).
- ✅ Feedback sonoro para coleta de pixels e game over usando buzzers distintos.
- ✅ LEDs de Status (Verde: Jogando, Azul: Pausado, Vermelho: Game Over).
- ✅ Botões físicos para Iniciar/Reiniciar e Pausar/Continuar o jogo.
- ✅ Botão do Joystick também funciona para Pausar/Continuar.
- ✅ Calibração automática do ponto central do joystick no início do jogo.
- ✅ Detecção de colisão com as bordas da tela (resulta em perda de vida).
- ✅ Período de imunidade temporária após perder uma vida.
- ✅ Saída serial (USB/UART) para depuração e acompanhamento do estado do jogo.

---

## 🛠️ Materiais Necessários

| Componente              | Quantidade | Observações                                      |
|-------------------------|------------|--------------------------------------------------|
| Raspberry Pi Pico W     | 1          | Pico normal também deve funcionar (sem WiFi)     |
| Display OLED SSD1306    | 1          | Interface I2C, 128x64 pixels                     |
| Matriz LED WS2812       | 1          | 5x5 (25 LEDs)                                    |
| Módulo Joystick Analógico | 1          | Com eixo X/Y (ADC) e botão integrado           |
| Botões de Pressão       | 2          | Para Start/Restart (B) e Pause/Resume (A)        |
| Buzzers                 | 2          | Para feedback sonoro (coleta e game over)       |
| LEDs                    | 3          | Verde, Azul, Vermelho (para status)             |
| Resistores              | 3          | ~220-330 Ohm (para os LEDs de status)          |
| Breadboard              | 1          | Para montagem do circuito                        |
| Jumpers Macho-Macho     | Vários     | Para conexões no breadboard                      |
| Cabo Micro USB          | 1          | Para alimentação e programação do Pico          |

---

## 🔌 Esquemático de Ligação (Pinos do Pico W)

| Componente            | Pino Pico | Descrição                      |
|-----------------------|-----------|--------------------------------|
| **Joystick**          |           |                                |
| Eixo X                | GP27      | ADC1                           |
| Eixo Y                | GP26      | ADC0                           |
| Botão (SW)            | GP22      | Input com Pull-up interno     |
| VCC                   | 3V3 (OUT) | Alimentação 3.3V               |
| GND                   | GND       | Terra                          |
| **Display OLED**      |           |                                |
| SDA                   | GP14      | I2C1 SDA                       |
| SCL                   | GP15      | I2C1 SCL                       |
| VCC                   | 3V3 (OUT) | Alimentação 3.3V               |
| GND                   | GND       | Terra                          |
| **Matriz LED WS2812** |           |                                |
| Data IN               | GP7       | Saída PIO                      |
| VCC                   | VBUS/VSYS | Alimentação 5V (Recomendado)*  |
| GND                   | GND       | Terra                          |
| **Botões**            |           |                                |
| Botão B (Start)       | GP6       | Input com Pull-up interno     |
| Botão A (Pause)       | GP5       | Input com Pull-up interno     |
| Lado Comum            | GND       | Conectar ao Terra              |
| **Buzzers**           |           |                                |
| Buzzer A (+)          | GP21      | Saída Digital (Som Pixel)     |
| Buzzer B (+)          | GP10      | Saída Digital (Som Game Over)  |
| Buzzers (-)           | GND       | Terra                          |
| **LEDs de Status**    |           |                                |
| LED Verde Anodo (+)   | GP11      | Saída Digital via Resistor     |
| LED Azul Anodo (+)    | GP12      | Saída Digital via Resistor     |
| LED Vermelho Anodo (+)| GP13      | Saída Digital via Resistor     |
| LEDs Catodo (-)       | GND       | Terra          |

*Nota sobre WS2812:* A alimentação da matriz (especialmente 5x5) pode exigir mais corrente do que o pino 3V3 do Pico pode fornecer. É recomendável alimentá-la diretamente pela fonte USB (VBUS) ou uma fonte externa de 5V, garantindo um GND comum com o Pico.

> ⚠️ **Atenção:** Verifique a pinagem exata dos seus componentes (OLED, Joystick) e a necessidade de níveis lógicos (3.3V do Pico).

 ![image](https://github.com/user-attachments/assets/37966e92-99bc-4c9d-aeac-f80c1e2e9524)


---

## 💡 Aplicações e Casos de Uso

- **Jogo Simples:** Uma demonstração de um jogo básico em hardware embarcado.
- **Aprendizado:** Excelente projeto para aprender a usar o Raspberry Pi Pico SDK, I2C, ADC, PIO (para WS2812), GPIOs e interrupções.
- **Integração de Componentes:** Mostra como integrar display, entrada analógica, LEDs endereçáveis, botões e som.
- **Prototipagem Rápida:** Base para jogos mais complexos ou interfaces interativas.

---

## 💻 Como Executar

### Pré-requisitos

1.  **Raspberry Pi Pico SDK:** Certifique-se de ter o ambiente de desenvolvimento C/C++ para o Pico configurado em sua máquina. Isso inclui `gcc-arm-none-eabi`, `cmake`, e `make`.
    *   Siga o guia oficial: [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
2.  **VS Code (Recomendado):** Com as extensões C/C++ e CMake Tools para facilitar o desenvolvimento e build.

### Passos

1.  **Clone o Repositório:**
    ```bash
    git clone <URL_DO_REPOSITORIO_AQUI>
    cd BitRun # Ou o nome da pasta do seu projeto
    ```

2.  **Configure o Projeto com CMake:**
    ```bash
    # Crie um diretório de build
    mkdir build
    cd build

    # Execute o CMake para configurar o projeto
    # Certifique-se que o caminho para o PICO_SDK_PATH está correto
    # (Pode ser necessário definir a variável de ambiente PICO_SDK_PATH)
    cmake .. 
    ```
    *Se o CMake não encontrar o SDK, você pode precisar especificá-lo:*
    ```bash
    cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk 
    ```

3.  **Compile o Projeto:**
    ```bash
    # Dentro do diretório 'build'
    make -j$(nproc) # Compila usando todos os núcleos disponíveis
    ```
    Isso gerará vários arquivos dentro de `build`, incluindo `bitrun.uf2` (ou similar, baseado no nome do executável definido no CMakeLists.txt, que atualmente é `Coletor_Pixels.uf2`).

4.  **Transfira para o Pico:**
    *   Desconecte o Pico da alimentação (USB).
    *   Pressione e segure o botão `BOOTSEL` no Pico.
    *   Conecte o Pico ao computador via USB enquanto mantém `BOOTSEL` pressionado.
    *   O Pico será montado como um dispositivo de armazenamento USB (RPI-RP2).
    *   Copie o arquivo `Coletor_Pixels.uf2` (ou `bitrun.uf2` se renomeado no CMakeLists) da pasta `build` para o dispositivo RPI-RP2.
    *   O Pico reiniciará automaticamente e executará o código.

---

## 🧪 Como Depurar ou Testar

1.  **Monitor Serial:** O projeto está configurado para usar `stdio` via USB (`pico_enable_stdio_usb(target 1)` no CMakeLists.txt).
    *   Use um terminal serial (como `minicom`, `picocom`, PuTTY, ou o monitor serial do Arduino IDE/PlatformIO) para conectar à porta serial virtual criada pelo Pico.
    *   A função `imprimir_estado_jogo()` envia informações úteis sobre o estado do joystick, posição do jogador, pontuação, vidas e estado do jogo para o serial a cada segundo durante o jogo.
2.  **LEDs de Status:** Os LEDs Verde, Azul e Vermelho fornecem uma indicação visual rápida do estado atual do jogo (Jogando, Pausado, Game Over).
3.  **Debug Clássico:** Use `printf` adicionais em pontos estratégicos do código para verificar valores de variáveis ou fluxo de execução. Recompile e transfira o `.uf2` após as modificações.

---

## 🎮 Como Usar (Gameplay)

1.  **Tela Inicial:** Ao ligar, o Pico exibirá uma tela inicial animada no OLED e o número máximo de vidas (3) na matriz de LED. O LED de status estará desligado ou piscando indicando espera.
2.  **Iniciar Jogo:** Pressione o **Botão B** para iniciar o jogo. Haverá uma breve calibração do joystick.
3.  **Jogar:**
    *   Use o **Joystick** para mover o quadrado branco (jogador) na tela OLED.
    *   O objetivo é coletar o pequeno quadrado (pixel) que aparece em locais aleatórios.
    *   Cada pixel coletado aumenta a pontuação (exibida no canto superior esquerdo do OLED) e um som curto é emitido pelo **Buzzer A**. O pixel reaparecerá em outro lugar.
    *   A matriz de LED mostra o número de vidas restantes (3, 2, 1 ou 0).
    *   O **LED Verde** acenderá indicando que o jogo está ativo.
4.  **Colisão com a Borda:** Se o jogador colidir com a borda da área de jogo, uma vida é perdida. O jogador ficará piscando por um curto período (imunidade) e a matriz de LED será atualizada.
5.  **Pausar/Continuar:** Pressione o **Botão A** ou o **Botão do Joystick** para pausar o jogo. A tela OLED mostrará "JOGO PAUSADO" e o **LED Azul** acenderá. A matriz de LED mostrará as vidas atuais em azul. Pressione o mesmo botão novamente para continuar.
6.  **Game Over:** Se as vidas chegarem a zero, o jogo termina. A tela OLED exibirá "GAME OVER" e a pontuação final. O **LED Vermelho** acenderá, a matriz de LED mostrará o número 0 em vermelho, e um som mais longo será emitido pelo **Buzzer B**.
7.  **Reiniciar:** Na tela de Game Over, pressione o **Botão B** para reiniciar o jogo (voltará à calibração e depois ao jogo com pontuação zerada e vidas cheias).

---

