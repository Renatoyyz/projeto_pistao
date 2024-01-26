#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
//#include <math.h>

#define INPUT_PIN 21       // Pino GPIO para a entrada com interrupção
#define OUTPUT_PIN 20      // Pino GPIO para a saída
#define LED_PIN 25

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define PWM_AVANCO 48.0f
#define PWM_RETRO 58.0f

volatile float pwm_percentage = 0.0;
volatile uint32_t start_time = 0;
volatile uint32_t duration = 0;
volatile float tempo_acionamento = 0;
bool status_acionamento = false;

uint32_t current_time = 0;
uint32_t pulse_duration = 0;
uint32_t start_local = 0;
uint32_t current_time_local = 0;

uint32_t time_cnt_geral = 0;

float off_set_percent = 0.97;

uint slice_num = 0;
uint16_t duty_cycle;

void interrupt_handler() {
    current_time = 0;
    pulse_duration = 0;
    start_local = 0;
    current_time_local = 0;

    start_time = time_us_32();
    while(gpio_get(INPUT_PIN)==1){;}
    current_time = time_us_32();
    pulse_duration = current_time - start_time;

    pwm_percentage = ((float)pulse_duration / 10.0f)*off_set_percent;  // Tempo em segundos convertido para porcentagem
    duration = pulse_duration;

    // Lógica para acionar ou desativar a saída
    if (pwm_percentage >= PWM_RETRO && status_acionamento == false) {
        gpio_put(OUTPUT_PIN, 1);  // Aciona a saída
        status_acionamento = true;
        //start_local = time_us_32();
    } else if(pwm_percentage <= PWM_AVANCO && status_acionamento == true){
        gpio_put(OUTPUT_PIN, 0);  // Desativa a saída
        status_acionamento=false;
        //current_time_local = time_us_32();
        tempo_acionamento = time_cnt_geral;
        time_cnt_geral=0;
        //tempo_acionamento = (current_time_local - start_local);
        //tempo_acionamento = ((tempo_acionamento/10000));
        //current_time_local=0;
        //start_local=0;
    }else{
       //gpio_put(OUTPUT_PIN, 0);  // Desativa a saída 
    }
}

void core1_entry(){
    while(true){
        if(status_acionamento==true){
            time_cnt_geral+=1;
        }else if(status_acionamento==false){
            //current_time_local+=
        }
        sleep_ms(1);
    }
}

void rs485_communication() {

    while (1) {

        // Aguarde comando do mestre
        char command[2];
        uart_read_blocking(UART_ID, (uint8_t *)command, sizeof(command));

        // Responda ao mestre com o valor do PWM quando solicitado
        if ((command[0] == 0x01) && (command[1] == 0x02) ) {  // ID: 1, Comando: 0
            char duration_response[40];
            snprintf(duration_response, sizeof(duration_response), "%.0f\n", tempo_acionamento);
            //snprintf(duration_response, sizeof(duration_response), "Valor: %.0f", round((double)pwm_percentage));
            uart_puts(UART_ID, duration_response);
            duration=0;
            tempo_acionamento=0;
        }
    }
}

int main() {    
    stdio_init_all();

    // Configura a frequência desejada (em Hz)
    uint32_t desired_frequency = 200000000; // Exemplo: 200 MHz
    // Configura o sistema para usar a nova frequência
    bool clk_ok = set_sys_clock_khz(desired_frequency / 1000, true);

    

    // Inicialização dos pinos
    // Inicializar o pino do LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(INPUT_PIN);
    gpio_set_dir(INPUT_PIN, GPIO_IN);

    gpio_init(OUTPUT_PIN);
    gpio_set_dir(OUTPUT_PIN, GPIO_OUT);

    // Configuração do UART para RS485
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    //bool clk_ok = true;

    if(clk_ok == true){
        gpio_put(LED_PIN, 1);
        sleep_ms(500);  // Aguardar 500 milissegundos (0,5 segundo)
        gpio_put(LED_PIN, 0);
        sleep_ms(500);  // Aguardar mais 500 milissegundos 
    }

    // Inicializar o núcleo
    multicore_launch_core1(core1_entry);
    gpio_set_irq_enabled(INPUT_PIN, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled_with_callback(INPUT_PIN, GPIO_IRQ_EDGE_RISE, true, &interrupt_handler);

    // Mantenha o programa principal em execução
    while (1) {
    rs485_communication();
    tight_loop_contents();
    }

    return 0;
}
