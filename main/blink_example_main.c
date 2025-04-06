#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"

#define Corriendo   1
#define Parado      0

#define LED_ROJO        0   
#define LED_VERDE       1

#define BOTON_TC0   2
#define BOTON_TC1   8
#define BOTON_TC2   9

#define DIGITO_ANCHO     60
#define DIGITO_ALTO      100
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO   0x3800
#define DIGITO_FONDO     ILI9341_BLACK

uint32_t Minutos = 0;
uint32_t Segundos = 0;
uint32_t Decimas = 0;


uint8_t Estado_global = Parado;
uint8_t Cambio_segundos = 0;
uint8_t Cambio_decimas = 0;

typedef struct Cronometro_s {
    uint32_t Delay; 
    uint32_t *puntero; 
    uint8_t Tope;
    SemaphoreHandle_t Semaforo;
    SemaphoreHandle_t Semaforo_uart;
    uint8_t *Cambio;
    panel_t panel_lcd;
}* Cronometro_t;

static void Parpadeo_led_verde (void *parameters){
    static uint8_t estado=1;
    while(1){
        if (Estado_global){
            gpio_set_level(LED_VERDE,estado);
            estado^=1;
            vTaskDelay(250/portTICK_PERIOD_MS);
            }
        else{
            vTaskDelay(250/portTICK_PERIOD_MS);
        }
        
    }
}

static void Chequeo_botones (void *parameters){

    while(1){
        if (gpio_get_level(BOTON_TC0)==0){
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (gpio_get_level(BOTON_TC0)==0 ){
                switch(Estado_global){
                    case Parado:
                    gpio_set_level(LED_ROJO,0);
                    Estado_global = Corriendo;
                    break;
                    case Corriendo:
                    Estado_global = Parado;
                    gpio_set_level(LED_VERDE,0);
                    gpio_set_level(LED_ROJO,1);
                    break;                    
                }
            }
            while(!gpio_get_level(BOTON_TC0)){}
        }
        else if (gpio_get_level(BOTON_TC1)==0){
            vTaskDelay(100/portTICK_PERIOD_MS);
            if ((gpio_get_level(BOTON_TC1)==0) && Estado_global==Parado){
                Minutos = 0;
                Segundos = 0;
                Decimas = 0;
            }
            while(!gpio_get_level(BOTON_TC1)){}
        }
        else if (gpio_get_level(BOTON_TC2)==0){
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (gpio_get_level(BOTON_TC2)==0){
                //printf("Minutos: %u/nSegundos: %u/nDecimas: %u",Minutos,Segundos,Decimas);
            }
            while(!gpio_get_level(BOTON_TC2)){}
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

static void Contador (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    // static TickType_t Tiempo_inicial = 0;
    // const TickType_t Periodo =  parametros->Delay;
    // Tiempo_inicial = xTaskGetTickCount();
    while(1){
        //Prueba para cambios de github

        if (Estado_global){
            vTaskDelay(parametros->Delay/portTICK_PERIOD_MS);
            xSemaphoreTake(parametros->Semaforo,portMAX_DELAY);
            *(parametros->puntero)+=1;
            if (*(parametros->puntero)==parametros->Tope){
                *(parametros->puntero)=0;
            }
            *(parametros->Cambio)=1;
            xSemaphoreGive(parametros->Semaforo);
            //vTaskDelayUntil(&Tiempo_inicial,Periodo);
        }
        else{
            vTaskDelay(100/portTICK_PERIOD_MS);
        }

    }
}

static void Actualizar_pantalla(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    uint16_t Conteo=0;
    while(1){
        if(*(parametros->Cambio) & Estado_global){
            xSemaphoreTake(parametros->Semaforo,parametros->Delay/portTICK_PERIOD_MS);
            xSemaphoreTake(parametros->Semaforo_uart,portMAX_DELAY);
            Conteo = *(parametros->puntero);
            *(parametros->Cambio)=0;
            xSemaphoreGive(parametros->Semaforo);
            uint8_t decena= Conteo/10;
            uint8_t unidad= Conteo%10;
            
            DibujarDigito(parametros->panel_lcd, 0, decena);
            DibujarDigito(parametros->panel_lcd, 1, unidad);

            ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
            ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);
            xSemaphoreGive(parametros->Semaforo_uart);
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void app_main (void){
    // Estos semáforos van a ser necesario cuando necesite actualizar la pantalla
    SemaphoreHandle_t  Semaforo_seg;
    SemaphoreHandle_t  Semaforo_dec;
    SemaphoreHandle_t  UART_Mutex;
    Semaforo_dec    =xSemaphoreCreateMutex();
    Semaforo_seg    =xSemaphoreCreateMutex();
    UART_Mutex      =xSemaphoreCreateMutex();

    // Declaración de la estructura para la task. Una sola tarea se encarga de contar todos los tiempos
    // tanto decimas, segundos como minutos. 
    static struct Cronometro_s Control_temporal [] = {
        {.Delay=60000, .puntero=&Minutos, .Tope=255},
        {.Delay=1000, .puntero=&Segundos, .Tope=60, .Cambio=&Cambio_segundos},
        {.Delay=10, .puntero=&Decimas, .Tope=100, .Cambio=&Cambio_decimas},
    }; 
    // Asignación de las variables de semáforo a la estructura. Debido a que no son constantes en tiempo
    // de ejecución, es necesario declararlas de esta manera.     
    Control_temporal[1].Semaforo=Semaforo_seg;
    Control_temporal[2].Semaforo=Semaforo_dec;
    Control_temporal[1].Semaforo_uart=UART_Mutex;
    Control_temporal[2].Semaforo_uart=UART_Mutex;
    
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    Control_temporal[1].panel_lcd=CrearPanel(30, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal[2].panel_lcd=CrearPanel(170, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    DibujarDigito(Control_temporal[1].panel_lcd, 0, 0);
    DibujarDigito(Control_temporal[1].panel_lcd, 1, 0);
    DibujarDigito(Control_temporal[2].panel_lcd, 0, 0);
    DibujarDigito(Control_temporal[2].panel_lcd, 1, 0);
    ILI9341DrawFilledCircle(160, 90, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(160, 130, 5, DIGITO_ENCENDIDO);

    gpio_config_t io_conf_int = {};
    io_conf_int.pin_bit_mask =
        ((1ULL << BOTON_TC0) | (1ULL << BOTON_TC1) | (1ULL << BOTON_TC2));
    io_conf_int.mode = GPIO_MODE_INPUT;
    io_conf_int.pull_up_en = true;
    io_conf_int.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int);

    gpio_config_t io_conf_int2 = {};
    io_conf_int2.pin_bit_mask =
        ((1ULL << LED_ROJO) | (1ULL << LED_VERDE));
    io_conf_int2.mode = GPIO_MODE_OUTPUT;
    io_conf_int2.pull_up_en = false;
    io_conf_int2.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int2);


    xTaskCreate(Parpadeo_led_verde,"Parpadeo_led",2*configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Chequeo_botones,"Chequeo",3*configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Contador,"Counter_seg",configMINIMAL_STACK_SIZE,(void*)&Control_temporal[1],tskIDLE_PRIORITY + 2,NULL);
    xTaskCreate(Contador,"Counter_dec",configMINIMAL_STACK_SIZE,(void*)&Control_temporal[2],tskIDLE_PRIORITY + 2,NULL);
    xTaskCreate(Actualizar_pantalla,"Actualizar_pantalla_dec",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal[2],tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Actualizar_pantalla,"Actualizar_pantalla_seg",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal[1],tskIDLE_PRIORITY + 1,NULL);

    }