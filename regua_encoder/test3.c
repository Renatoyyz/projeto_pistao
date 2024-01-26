#include <stdio.h>
//#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#define INPUT_PIN 21       // Pino GPIO para a entrada com interrupção
#define OUTPUT_PIN 20      // Pino GPIO para a saída
#define LED_PIN 25

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define PISTAO_RECUADO 0
#define PISTAO_AVANCADO 1

char info_response[40];
volatile uint32_t cnt_encoder_role = 0;
bool habilita_rotina = false;
char flag_pistao = PISTAO_RECUADO;
bool status_periodo = false;

char movimento_encoder = 1;

volatile uint32_t cnt_timer_avanco = 0;
volatile uint32_t cnt_timer_recuo = 0;

//Parei no tratamento dos cnt_timer_avanco e cnt_timer_recuo

void interrupt_handler() {

    cnt_encoder_role += 1;
    movimento_encoder = 1;
    if( cnt_encoder_role > 1000000000 ){
        cnt_encoder_role=0;
    }
}

void core1_timer(){
    while(true){
        sleep_ms(20);
        if(habilita_rotina == true && status_periodo == false){
            if( flag_pistao == PISTAO_RECUADO ){
                gpio_put(OUTPUT_PIN, 1);
                
                if(movimento_encoder == 1){
                    movimento_encoder=0;
                }else if(cnt_encoder_role > 0){
                    cnt_timer_avanco = cnt_encoder_role;
                    flag_pistao = PISTAO_AVANCADO;
                    cnt_encoder_role = 0;
                    movimento_encoder = 1;
                }else{
                    movimento_encoder = 1;
                }
            } else if( flag_pistao == PISTAO_AVANCADO ){
                
                  gpio_put(OUTPUT_PIN, 0);
                  
                
                if(movimento_encoder == 1){
                    movimento_encoder=0;
                }else if(cnt_encoder_role > 0){
                    cnt_timer_recuo = cnt_encoder_role;
                    flag_pistao = PISTAO_RECUADO;
                    cnt_encoder_role = 0;
                    status_periodo = true;// finalizou esse período
                    movimento_encoder = 1;
                }else{
                    movimento_encoder = 1;
                }
            
            }
        }

    sleep_ms(20);
    }
}

void pistao(bool atv){
        gpio_put(OUTPUT_PIN, atv);
        snprintf(info_response, sizeof(info_response), "%s", "-1\n");
        uart_puts(UART_ID, info_response);
        if( atv == 0 ){
            flag_pistao = PISTAO_RECUADO;
        }else if( atv == 1 ){
            flag_pistao = PISTAO_AVANCADO;
        }
}

void rs485_communication() {

    while (1) {
        // Aguarde comando do mestre
        char command[2];
        
        uart_read_blocking(UART_ID, (uint8_t *)command, sizeof(command));

        // Responda ao mestre com o valor do PWM quando solicitado
        if ((command[0] == 0x01) ) {  // ID: 1, Comando: 0

        switch(command[1]){
            case 1://Acionar Pistão
            pistao(1);
            sleep_ms(2000);
            cnt_encoder_role = 0; //zera o contador de encoder
            break;

            case 2://desaciona pistão
            pistao(0);
            sleep_ms(2000);
            cnt_encoder_role = 0; //zera o contador de encoder
            break;

            case 3://Calibrar ponto mínimo
            cnt_encoder_role = 0; //zera o contador de encoder
            snprintf(info_response, sizeof(info_response), "%ld\n", cnt_encoder_role);
            uart_puts(UART_ID, info_response);
            break;

            case 4://Calibrar ponto máximo
            //Envia valor de pulsos de encoder para mestre
            snprintf(info_response, sizeof(info_response), "%ld\n", cnt_encoder_role);
            uart_puts(UART_ID, info_response);
            cnt_encoder_role = 0;// Zera novamente o contador da regua.
            break;

            case 5://Iniciar rotina
            pistao(0);//Recua pistão
            snprintf(info_response, sizeof(info_response), "%ld\n", cnt_encoder_role);
            uart_puts(UART_ID, info_response);
            sleep_ms(2000);//Aguarda um tempo
            cnt_encoder_role = 0; //zera o contador de encoder
            habilita_rotina = true;//Habilita a rotina
            break;

            case 6://Parar rotina
            pistao(0);//Recua pistão
            snprintf(info_response, sizeof(info_response), "%ld\n", cnt_encoder_role);
            uart_puts(UART_ID, info_response);
            sleep_ms(2000);//Aguarda um tempo
            cnt_encoder_role = 0; //zera o contador de encoder
            habilita_rotina = false;//Desabilita a rotina
            status_periodo = false;
            break;

            default:
            if(habilita_rotina == true && status_periodo == true ){
                snprintf(info_response, sizeof(info_response), "%ld;%ld\n", cnt_timer_avanco,cnt_timer_recuo);
                uart_puts(UART_ID, info_response);
                status_periodo = false;//Habilita para uma nova leitura...
                movimento_encoder = 1;
            }
        }
        }
    }
}

int main() {    
    stdio_init_all();

    // Configura a frequência desejada (em Hz)
    uint32_t desired_frequency = 200000000; // Exemplo: 200 MHz
    // Configura o sistema para usar a nova frequência
    bool clk_ok = false;
    //clk_ok = set_sys_clock_khz(desired_frequency / 1000, true);
    clk_ok = true;

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

    if(clk_ok == true){
        gpio_put(LED_PIN, 1);
        sleep_ms(500);  // Aguardar 500 milissegundos (0,5 segundo)
        gpio_put(LED_PIN, 0);
        sleep_ms(500);  // Aguardar mais 500 milissegundos 
    }

    // Inicializar o núcleo
    multicore_launch_core1(core1_timer);
    gpio_set_irq_enabled(INPUT_PIN, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled_with_callback(INPUT_PIN, GPIO_IRQ_EDGE_RISE, true, &interrupt_handler);

    // Mantenha o programa principal em execução
    while (1) {
    rs485_communication();
    //tight_loop_contents();
    if(habilita_rotina==true){
            if( flag_pistao == PISTAO_RECUADO ){
                gpio_put(OUTPUT_PIN, 1);
            }
        }
    }

    return 0;
}
