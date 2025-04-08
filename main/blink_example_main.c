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
uint32_t Conteo = 0;

uint8_t Estado_global = Parado;

typedef struct Cronometro_s {
    SemaphoreHandle_t Semaforo;
    SemaphoreHandle_t Semaforo_g;
    panel_t panel_lcd_dec;
    panel_t panel_lcd_min;
    panel_t panel_lcd_seg;
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
    Cronometro_t parametros = (Cronometro_t) parameters;
    while(1){
        if (gpio_get_level(BOTON_TC0)==0){
            vTaskDelay(50/portTICK_PERIOD_MS);
            while(!gpio_get_level(BOTON_TC0)){}
            xSemaphoreTake(parametros->Semaforo_g,portMAX_DELAY);
                switch(Estado_global){
                    case Parado:
                    Estado_global = Corriendo;
                    xSemaphoreGive(parametros->Semaforo_g);
                    gpio_set_level(LED_ROJO,0);
                    break;
                    case Corriendo:
                    Estado_global = Parado;
                    xSemaphoreGive(parametros->Semaforo_g);
                    gpio_set_level(LED_VERDE,0);
                    gpio_set_level(LED_ROJO,1);
                    break;                    
                }
        }
        else if (gpio_get_level(BOTON_TC1)==0){
            vTaskDelay(50/portTICK_PERIOD_MS);
            while(!gpio_get_level(BOTON_TC1)){}
            xSemaphoreTake(parametros->Semaforo_g,portMAX_DELAY);
            if ((gpio_get_level(BOTON_TC1)==0) && Estado_global==Parado){
                Conteo = 0;
            }
            xSemaphoreGive(parametros->Semaforo_g);

        }
        else if (gpio_get_level(BOTON_TC2)==0){
            if (gpio_get_level(BOTON_TC2)==0){
                //printf("Minutos: %u/nSegundos: %u/nDecimas: %u",Minutos,Segundos,Decimas);
            }
            while(!gpio_get_level(BOTON_TC2)){}
            vTaskDelay(50/portTICK_PERIOD_MS);
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

static void Contador (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static TickType_t Tiempo_inicial = 0;
    Tiempo_inicial = xTaskGetTickCount();
    while(1){
        xSemaphoreTake(parametros->Semaforo_g,portMAX_DELAY);
        if (Estado_global){
            xSemaphoreGive(parametros->Semaforo_g);
            xSemaphoreTake(parametros->Semaforo,portMAX_DELAY);
            Conteo+=1;
            xSemaphoreGive(parametros->Semaforo);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
        }
        else{
            xSemaphoreGive(parametros->Semaforo_g);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
        }
    }
}

static void Actualizar_pantalla(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint16_t Conteo_previo=0;
    while(1){
        xSemaphoreTake(parametros->Semaforo,portMAX_DELAY);
        if (Conteo_previo!=Conteo){
            Conteo_previo=Conteo;
            xSemaphoreGive(parametros->Semaforo);
            Decimas=Conteo%10;
            Segundos=Conteo/10;
            Minutos=Segundos/60;
            DibujarDigito(parametros->panel_lcd_dec, 0, Decimas);
            uint8_t decena_seg=(Segundos%60)/10;
            uint8_t unidad_seg=(Segundos%60)%10;
            DibujarDigito(parametros->panel_lcd_seg, 0, decena_seg);
            DibujarDigito(parametros->panel_lcd_seg, 1, unidad_seg);
            DibujarDigito(parametros->panel_lcd_min, 0, Minutos);            
        }
        else{
            xSemaphoreGive(parametros->Semaforo);
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
        }
}

void app_main (void){
    SemaphoreHandle_t  Semaforo_global; 
    SemaphoreHandle_t Semaforo_estado;
    Semaforo_global    =xSemaphoreCreateMutex();
    Semaforo_estado    =xSemaphoreCreateMutex();
    static struct Cronometro_s Control_temporal;  
    Control_temporal.Semaforo=Semaforo_global; //Hace referencia a la variable de Conteo. Actualizado en la task de "Contador"
    Control_temporal.Semaforo_g=Semaforo_estado; //Hace referencia a la variable Estado, Corriendo o Parado

    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    Control_temporal.panel_lcd_min=CrearPanel(10, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.panel_lcd_seg=CrearPanel(90, 60, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.panel_lcd_dec=CrearPanel(230, 60, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
   
    DibujarDigito(Control_temporal.panel_lcd_dec, 0, 0);
    DibujarDigito(Control_temporal.panel_lcd_seg, 0, 0);
    DibujarDigito(Control_temporal.panel_lcd_seg, 1, 0);
    DibujarDigito(Control_temporal.panel_lcd_min, 0, 0);
    ILI9341DrawFilledCircle(80, 150, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(220, 150 , 5, DIGITO_ENCENDIDO);

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
    xTaskCreate(Chequeo_botones,"Chequeo",3*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Contador,"Counter_seg",configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 3,NULL);
    xTaskCreate(Actualizar_pantalla,"Actualizar_pantalla_dec",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 2,NULL);

    }