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

#define INPUT_PIN 21       // Pino GPIO para a entrada com interrupção
#define OUTPUT_PIN 20      // Pino GPIO para a saída
#define LED_PIN 25

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define PISTAO_RECUADO 0
#define PISTAO_AVANCADO 1

//Constante para definir um delay para o inicio do avanço ou recuo do pistão
#define DELAY_AV_RE 100

char info_response[40];
volatile uint32_t cnt_encoder_role = 0;
char flag_pistao = PISTAO_RECUADO;

//
#define STATUS_HAB 1
#define STATUS_DESABI 2
#define STATUS_DESATI 0
char inicia_para_moviento_avanco = STATUS_DESATI;
char inicia_para_moviento_recuo = STATUS_DESATI;
bool status_encoder = false;
bool movimento_encoder = true;

volatile uint32_t cnt_timer_avanco = 0;
volatile uint32_t cnt_timer_recuo = 0;

volatile uint32_t encoder_role_avanco = 0;
volatile uint32_t encoder_role_recuo = 0;

//Parei no tratamento dos cnt_timer_avanco e cnt_timer_recuo

void interrupt_handler() {

    cnt_encoder_role += 1;
    movimento_encoder = true;
    if( cnt_encoder_role > 1000000000 ){
        cnt_encoder_role=0;
    }
}

void core1_timer(){
    while(true){
        // sleep_ms(3);
        sleep_us(500);
        if(inicia_para_moviento_avanco == STATUS_HAB){// se iniciou movimento
            cnt_timer_avanco+=1;// conta tempo, em ms, do tempo de avanço
            if( movimento_encoder == true ){// se encoder estiver em movimento 
                movimento_encoder = false; 
                }
            else{
                    // inicia_para_moviento_avanco = STATUS_DESABI;// encoder estiver parado, para variavel de inicio de movimento
                    // encoder_role_avanco = cnt_encoder_role;
                sleep_ms(10);
                if(movimento_encoder==false){
                    inicia_para_moviento_avanco = STATUS_DESABI;// encoder estiver parado, para variavel de inicio de movimento
                    encoder_role_avanco = cnt_encoder_role;
                }else{movimento_encoder = false;}
                
                }
        }
        if(inicia_para_moviento_recuo == STATUS_HAB){// se iniciou movimento
            cnt_timer_recuo+=1;// conta tempo, em ms, do tempo de recuo
            if( movimento_encoder == true ){// se encoder estiver em movimento 
                movimento_encoder = false;//refresha
                }
            else{
                    // inicia_para_moviento_recuo = STATUS_DESABI;// encoder estiver parado, para variavel de inicio de movimento
                    // encoder_role_recuo = cnt_encoder_role;
                sleep_ms(10);
                if(movimento_encoder==false){
                    inicia_para_moviento_recuo = STATUS_DESABI;// encoder estiver parado, para variavel de inicio de movimento
                    encoder_role_recuo = cnt_encoder_role;
                }else{movimento_encoder = false;}
                }
        }
    // sleep_ms(1);
    sleep_us(500);
    }
}

volatile uint32_t pistao(bool atv){
        gpio_put(OUTPUT_PIN, atv);
        if( atv == 1 ){
            flag_pistao = PISTAO_AVANCADO;
            snprintf(info_response, sizeof(info_response), "%s", "-1\n");// -1 indica, para o mestre, avanço.
            uart_puts(UART_ID, info_response);
        }else if( atv == 0 ){
            flag_pistao = PISTAO_RECUADO;
            snprintf(info_response, sizeof(info_response), "%s", "-2\n");// -2 indica, para o mestre, recuo.
            uart_puts(UART_ID, info_response);
        }
        return cnt_encoder_role;// retorna valor do encoder
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

            case 5://Iniciar medida de avanço
            inicia_para_moviento_avanco = STATUS_DESATI;
            inicia_para_moviento_recuo = STATUS_DESATI;
            pistao(1);//Avança pistão 
            cnt_encoder_role = 0; //zera o contador de encoder
            sleep_ms(DELAY_AV_RE);//espera-se um tempo para estabilizar
            movimento_encoder = true;//Habilita a variável de movimento previamente.
            inicia_para_moviento_avanco = STATUS_HAB;// Habilita movimento de avanço
            inicia_para_moviento_recuo= STATUS_DESATI;// Desativa movimento de recuo
            encoder_role_avanco=0;//Zera variáveis de medidas
            cnt_timer_avanco=0;
            break;

            case 6://Inicia madida de recuo
            inicia_para_moviento_avanco = STATUS_DESATI;
            inicia_para_moviento_recuo = STATUS_DESATI;
            pistao(0);//Recua pistão
            cnt_encoder_role = 0; //zera o contador de encoder
            sleep_ms(DELAY_AV_RE);//espera-se um tempo para estabilizar
            movimento_encoder = true;//Habilita a variável de movimento previamente.
            inicia_para_moviento_recuo= STATUS_HAB;// Habilita movimento de recuo
            inicia_para_moviento_avanco = STATUS_DESATI;// Habilita movimento de avanço
            encoder_role_recuo=0;//Zera variáveis de medidas
            cnt_timer_recuo=0;
            break;

            case 7:
            pistao(0);//Recua pistão
            sleep_ms(3000);//espera-se um tempo para recuar
            movimento_encoder = true;//Habilita a variável de movimento previamente.
            cnt_encoder_role = 0; //zera o contador de encoder
            inicia_para_moviento_recuo= STATUS_DESATI;// Desativa movimentos
            inicia_para_moviento_avanco = STATUS_DESATI;// Desativa movimentos
            encoder_role_recuo=0;//Zera variáveis de medidas
            encoder_role_avanco=0;//Zera variáveis de medidas
            cnt_timer_recuo=0;
            cnt_timer_avanco=0;
            break;

            default:
                if( inicia_para_moviento_avanco == STATUS_DESABI){// Se 

                    snprintf(info_response, sizeof(info_response), "%ld;%ld\n", encoder_role_avanco,(cnt_timer_avanco*1));
                    uart_puts(UART_ID, info_response);

                }else if(inicia_para_moviento_recuo == STATUS_DESABI){
                    snprintf(info_response, sizeof(info_response), "%ld;%ld\n", encoder_role_recuo,(cnt_timer_recuo*1));
                    uart_puts(UART_ID, info_response);
                }else{
                    snprintf(info_response, sizeof(info_response), "%s\n", "-3\n");//indica que ainda não terminou a medida
                    uart_puts(UART_ID, info_response);
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
    clk_ok = set_sys_clock_khz(desired_frequency / 1000, true);
    // clk_ok = true;

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
    tight_loop_contents();
    }

    return 0;
}
