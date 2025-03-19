#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <string.h>
#include <stdlib.h>
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define LED_R_PIN 13
#define LED_G_PIN 11
#define LED_B_PIN 12
#define botaoJoy 22
#define I2C_SDA 14
#define I2C_SCL 15
#define BUZZER_PIN 21
#define PERIOD 2000
#define LED_STEP 100
#define DIVIDER_PWM 16.0

// Variáveis globais
uint16_t led_level = 100;
bool up_down = true;
uint8_t ssd[ssd1306_buffer_length];
int indiceOpcao = 0;
bool exibindoMensagem = false;

// Área de renderização
struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1
};

//Mensagens do menu
char *opcoes[] = { "1. Joystick LED", "2. Tocar buzzer", "3. LED RGB PWM" };
char *opcao1[] = { "Joystick LED", "-", "Aperte o joy", "para sair." };
char *opcao2[] = { "Tocar buzzer", "-", "Aperte o joy", "para sair." };
char *opcao3[] = { "LED RGB PWM", "-", "Aperte o joy", "para sair." };

//Protótipo das funções para deixar o código mais organizado e o main ficar em cima
int WaitWithRead(int timeMS);
void desenhar_borda(int linha_inicio);
void show_menu(int indice_selecionado);
void show_message(char *text[]);
void beep();
void ledjoy();
void pwm_init_buzzer(uint pin);
void play_tone(uint pin, uint frequency, uint duration_ms);
void tocarBuzzer();
void set_leds(bool red, bool green, bool blue);
void pwmLed();

int main()
{
    stdio_init_all();

    adc_init();
    adc_gpio_init(26); //eixo X
    adc_gpio_init(27); //eixo Y

    gpio_init(botaoJoy);
    gpio_set_dir(botaoJoy, GPIO_IN);
    gpio_pull_up(botaoJoy);

    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);

    pwm_init_buzzer(BUZZER_PIN);

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
    calculate_render_area_buffer_length(&frame_area);
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    show_menu(indiceOpcao);

    while (1)
    {
        if (!exibindoMensagem)
        {
            adc_select_input(0);
            uint adc_y_raw = adc_read();

            if (adc_y_raw > 4000)
            {
                indiceOpcao = (indiceOpcao - 1 + 3) % 3;
                show_menu(indiceOpcao);
                sleep_ms(200);
            }
            else if (adc_y_raw < 100)
            {
                indiceOpcao = (indiceOpcao + 1) % 3;
                show_menu(indiceOpcao);
                sleep_ms(200);
            }
        }

        if (!gpio_get(botaoJoy))
        {
            exibindoMensagem = !exibindoMensagem;
            sleep_ms(200);

            if (exibindoMensagem)
            {
                beep();
                switch (indiceOpcao)
                {
                    case 0:
                        show_message(opcao1);
                        ledjoy();
                        break;
                    case 1:
                        show_message(opcao2);
                        tocarBuzzer();
                        break;
                    case 2:
                        show_message(opcao3);
                        pwmLed();
                        break;
                }

            }
            else
            {
                show_menu(indiceOpcao);
            }
        }
    }
    return 0;
}

//------------------------------FUNÇÕES AUXILIARES------------------------------------------------

// Função para aguardar um tempo em milissegundos, mas que pode ser interrompida pelo botão
int WaitWithRead(int timeMS)
{
    for (int i = 0; i < timeMS; i += 50)
    {
        if (!gpio_get(botaoJoy))
        {
            sleep_ms(10);
            return 1;
        }
        sleep_ms(10);
    }
    return 0;
}

// Função para desenhar borda de seleção na tela
void desenhar_borda(int linha_inicio)
{
    int largura = 2;
    for (int i = 0; i < largura; i++)
    {
        ssd1306_draw_line(ssd, 0, linha_inicio + i, ssd1306_width - 1, linha_inicio + i, 1);
        ssd1306_draw_line(ssd, 0, linha_inicio + 7 + i, ssd1306_width - 1, linha_inicio + 7 + i, 1);
    }
    for (int i = 0; i < largura; i++)
    {
        ssd1306_draw_line(ssd, 0 + i, linha_inicio, 0 + i, linha_inicio + 7, 1);
        ssd1306_draw_line(ssd, ssd1306_width - 1 - i, linha_inicio, ssd1306_width - 1 - i, linha_inicio + 7, 1);
    }
}

// Função para exibir o menu na tela
void show_menu(int indice_selecionado)
{
    memset(ssd, 0, ssd1306_buffer_length);
    for (int i = 0; i < 3; i++)
    {
        ssd1306_draw_string(ssd, 5, i * 16, opcoes[i]);
    }
    desenhar_borda(indice_selecionado * 16);
    render_on_display(ssd, &frame_area);
}

// Função para exibir mensagem na tela
void show_message(char *text[]) {
    memset(ssd, 0, ssd1306_buffer_length);
    for (int i = 0; i < 4; i++) {
        ssd1306_draw_string(ssd, 5, i * 8, text[i]);
    }
    render_on_display(ssd, &frame_area);
}

// Função para controlar o LED RGB com o joystick (mais simplificada)
void ledjoy()
{
    while (1)
    {
        adc_select_input(0);
        uint adc_y_raw = adc_read();
        adc_select_input(1);
        uint adc_x_raw = adc_read();

        pwm_set_enabled(pwm_gpio_to_slice_num(LED_B_PIN), false);
        gpio_set_function(LED_B_PIN, GPIO_FUNC_SIO);

        if(adc_x_raw > 3500)
        {
            set_leds(1,0,0);
        } 
        else if(adc_x_raw < 200)
        {
            set_leds(0,0,1);
        }
        else if(adc_y_raw > 3500)
        {
            set_leds(0,1,0);
        } 
        else if(adc_y_raw < 200)
        {
            set_leds(1,1,0);
        }
        else
        {
            set_leds(1,1,1);
            if (WaitWithRead(100)) break;
        }
    }
    set_leds(0,0,0);
}

// Função para inicializar o buzzer
void pwm_init_buzzer(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
}

// Função para emitir um beep massa ao apertar o botão e entrar em uma opção
void beep() 
{
    play_tone(BUZZER_PIN, 220, 100);
    play_tone(BUZZER_PIN, 440, 100);
}

// Função para tocar uma nota musical
void play_tone(uint pin, uint frequency, uint duration_ms)
{
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2);

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0);
    sleep_ms(10);
}

// Notas musicais para a música tema de Star Wars
const uint star_wars_notes[] =
{
    330, 330, 330, 262, 392, 523, 330, 262,
    392, 523, 330, 659, 659, 659, 698, 523,
    415, 349, 330, 262, 392, 523, 330, 262,
    392, 523, 330, 659, 659, 659, 698, 523,
    415, 349, 330, 523, 494, 440, 392, 330,
    659, 784, 659, 523, 494, 440, 392, 330,
    659, 659, 330, 784, 880, 698, 784, 659,
    523, 494, 440, 392, 659, 784, 659, 523,
    494, 440, 392, 330, 659, 523, 659, 262,
    330, 294, 247, 262, 220, 262, 330, 262,
    330, 294, 247, 262, 330, 392, 523, 440,
    349, 330, 659, 784, 659, 523, 494, 440,
    392, 659, 784, 659, 523, 494, 440, 392
};

// Duração das notas em milissegundos
const uint note_duration[] =
{
    500, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 500, 500, 500, 350, 150,
    300, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 650, 500, 150, 300, 500, 350,
    150, 300, 500, 150, 300, 500, 350, 150,
    300, 650, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 500, 500, 500, 350, 150,
    300, 500, 500, 350, 150, 300, 500, 350,
    150, 300, 500, 350, 150, 300, 500, 500,
    350, 150, 300, 500, 500, 350, 150, 300,
};

// Função para tocar a música tema de Star Wars e ser chamada na opção 2
void tocarBuzzer()
{
    for (int i = 0; i < sizeof(star_wars_notes) / sizeof(star_wars_notes[0]); i++)
    {
        int duration = note_duration[i];
        uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
        uint32_t clock_freq = clock_get_hz(clk_sys);
        uint32_t top = clock_freq / star_wars_notes[i] - 1;

        pwm_set_wrap(slice_num, top);
        pwm_set_gpio_level(BUZZER_PIN, top / 2);

        int elapsed = 0;
        while (elapsed < duration)
        {
            if (!gpio_get(botaoJoy))
            {
                pwm_set_gpio_level(BUZZER_PIN, 0);
                return;
            }
            sleep_ms(10);
            elapsed += 10;
        }

        pwm_set_gpio_level(BUZZER_PIN, 0);
        elapsed = 0;
        while (elapsed < 50)
        {
            if (!gpio_get(botaoJoy))
            {
                return;
            }
            sleep_ms(10);
            elapsed += 10;
        }
    }
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Função para setar os LEDs RGB
void set_leds(bool red, bool green, bool blue)
{
    gpio_put(LED_R_PIN, red);
    gpio_put(LED_G_PIN, green);
    gpio_put(LED_B_PIN, blue);
}

// Função para controlar o LED RGB com PWM
void pwmLed()
{
    uint slice = pwm_gpio_to_slice_num(LED_B_PIN);
    gpio_set_function(LED_B_PIN, GPIO_FUNC_PWM);
    pwm_set_clkdiv(slice, DIVIDER_PWM);
    pwm_set_wrap(slice, PERIOD);
    pwm_set_enabled(slice, true);

    while (1)
    {
        pwm_set_gpio_level(LED_B_PIN, led_level);
        sleep_ms(30);

        if (up_down)
        {
            led_level += LED_STEP;
            if (led_level >= PERIOD) up_down = false;
        }
        else
        {
            led_level -= LED_STEP;
            if (led_level <= LED_STEP) up_down = true;
        }
        if (WaitWithRead(100)) break;
    }
    pwm_set_gpio_level(LED_B_PIN, 0);
}