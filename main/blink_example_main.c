#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"

#define LED_ROJO        0   
#define LED_VERDE       1

#define CORRIENDO       1
#define PARADO          0

#define BOTON_TC0   2
#define BOTON_TC1   8
#define BOTON_TC2   9

#define Interrupcion_TC0 (1 << 1)
#define Interrupcion_TC1 (1 << 2)
#define Interrupcion_TC2 (1 << 3)
#define Cambio_min (1 << 4)
#define Cambio_seg (1 << 5)
#define Cambio_dec (1 << 6)
#define Resetear_Ev (1 << 7)
#define Pantalla_Lap (1 << 8)
#define Pantalla_dec (1 << 9)
#define Pantalla_seg (1 << 10)
#define Pantalla_min (1 << 11)

#define DIGITO_ANCHO     40
#define DIGITO_ALTO      80
#define LAP_ANCHO        25
#define LAP_ALTO         50
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO   0x3800
#define DIGITO_FONDO     ILI9341_BLACK

uint32_t Conteo = 0;
uint32_t Decimas=0;
uint32_t Segundos=0;
uint32_t Minutos=0;

uint8_t Estado_Global=0;

typedef struct Cronometro_s {
    SemaphoreHandle_t Semáforo_variable; //Va a controlar el acceso a la variable de Estado_global
    SemaphoreHandle_t Semaforo_Estado_G;
    SemaphoreHandle_t Semaforo_UART;
    SemaphoreHandle_t Semáforo_dec;
    SemaphoreHandle_t Semáforo_seg;
    SemaphoreHandle_t Semáforo_min;
    panel_t Panel_dec;
    panel_t Panel_seg;
    panel_t Panel_min;
    EventGroupHandle_t Eventos_task;
}* Cronometro_t;

typedef struct Pantalla_Lap_s {
    panel_t Panel_lap1;
    panel_t Panel_lap2;
} *Pantalla_Lap_t;

void Testeo_Botones(void *args){
    Cronometro_t Evento = (Cronometro_t) args;
    while(1){
        if (!gpio_get_level(BOTON_TC0))
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC0)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupSetBits(Evento->Eventos_task,Interrupcion_TC0);
        }
        else if (!gpio_get_level(BOTON_TC1))
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC1)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupSetBits(Evento->Eventos_task,Interrupcion_TC1);
        }
        else if (!gpio_get_level(BOTON_TC2))
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC2)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupSetBits(Evento->Eventos_task,Interrupcion_TC2);
        }
            vTaskDelay(100 / portTICK_PERIOD_MS);
    }    
}

static void Parpadeo_led_verde (void *parameters){
    static uint8_t estado=1;
    while(1){
        if (Estado_Global){
            gpio_set_level(LED_VERDE,estado);
            estado^=1;
            vTaskDelay(250/portTICK_PERIOD_MS);
            }
        else{
            vTaskDelay(250/portTICK_PERIOD_MS);
        }
        
    }
}

static void Start_Stop_Cronometro (void *parameters){
    Cronometro_t parametros= (Cronometro_t) parameters;
    EventBits_t Bits_return = 0;
    while(1){
        Bits_return = xEventGroupWaitBits(parametros->Eventos_task,
                                            Interrupcion_TC0,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);
        if ((Bits_return & Interrupcion_TC0) != 0) {
            xSemaphoreTake(parametros->Semaforo_Estado_G,portMAX_DELAY);
            if (Estado_Global == PARADO){
                Estado_Global = CORRIENDO;
                gpio_set_level(LED_ROJO,0);
            }
            else{
                Estado_Global = PARADO;
                gpio_set_level(LED_VERDE,0);
                gpio_set_level(LED_ROJO,1);
            }
            xSemaphoreGive(parametros->Semaforo_Estado_G);
        }
    }
}
static void Reset_Cronometro (void *parameters){
    Cronometro_t Estructura = (Cronometro_t) parameters;
    EventBits_t Bits_return = 0;
    while(1){
        Bits_return = xEventGroupWaitBits(Estructura->Eventos_task,
                            Interrupcion_TC1,
                            pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);
        if ((Bits_return & Interrupcion_TC1) != 0) {
            xSemaphoreTake(Estructura->Semaforo_Estado_G,portMAX_DELAY);
            if (Estado_Global == PARADO){
                xSemaphoreTake(Estructura->Semáforo_variable,portMAX_DELAY);
                Conteo=0;
                xSemaphoreGive(Estructura->Semáforo_variable);
                xSemaphoreGive(Estructura->Semaforo_Estado_G);
                xEventGroupSetBits(Estructura->Eventos_task,(Cambio_dec|Cambio_min|Cambio_seg));
            }
            else{
                xSemaphoreGive(Estructura->Semaforo_Estado_G);
            }
        }
    }
}

// static void Obtener_Lap (void *parameters){
//     Cronometro_t Estructura = (Cronometro_t) parameters;
//     EventBits_t Bits_return = 0;
//     while(1){
//         Bits_return = xEventGroupWaitBits(Estructura->Eventos_task,
//                             Interrupcion_TC2,
//                             pdTRUE,
//                             pdFALSE,
//                             portMAX_DELAY);
//         if ((Bits_return & Interrupcion_TC2) == Interrupcion_TC2) {

static void Contador (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    TickType_t Tiempo_inicial = 0;
    Tiempo_inicial = xTaskGetTickCount();
    static uint32_t Conteo_local=0;
    while(1){
        xSemaphoreTake(parametros->Semaforo_Estado_G,portMAX_DELAY);
        if (Estado_Global){
            xSemaphoreGive(parametros->Semaforo_Estado_G);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
            xSemaphoreTake(parametros->Semáforo_variable,portMAX_DELAY);
            Conteo+=1;
            Conteo_local=Conteo;
            xSemaphoreGive(parametros->Semáforo_variable);
            xEventGroupSetBits(parametros->Eventos_task,Cambio_dec);
            if (((Conteo_local%10)==0) && (Conteo_local!=0)) {
                xEventGroupSetBits(parametros->Eventos_task,Cambio_seg);
            }
            if (((Conteo_local%600)==0) && (Conteo_local!=0)) {
                xEventGroupSetBits(parametros->Eventos_task,Cambio_min);
            }
        }
        else{
            xSemaphoreGive(parametros->Semaforo_Estado_G);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
        }
    }
}

static void Actualizar_valores(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint32_t Conteo_local = 0;
    EventBits_t Bits_Evento = 0;
    while(1){
        Bits_Evento = xEventGroupWaitBits(
            parametros->Eventos_task,    
            Cambio_dec | Cambio_seg | Cambio_min,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
        xSemaphoreTake(parametros->Semáforo_variable,portMAX_DELAY);
        Conteo_local = Conteo;
        xSemaphoreGive(parametros->Semáforo_variable);
        if ((Bits_Evento & Cambio_dec) != 0){
            xSemaphoreTake(parametros->Semáforo_dec,portMAX_DELAY);
            Decimas = Conteo_local%10;
            xSemaphoreGive(parametros->Semáforo_dec);
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_dec);
        }
        if ((Bits_Evento & Cambio_seg) != 0){
            xSemaphoreTake(parametros->Semáforo_seg,portMAX_DELAY);
            Segundos = (Conteo_local/10)%60;
            xSemaphoreGive(parametros->Semáforo_seg);
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_seg);
        }
        if ((Bits_Evento & Cambio_min) != 0){
            xSemaphoreTake(parametros->Semáforo_min,portMAX_DELAY);
            Minutos = Conteo_local/600;
            xSemaphoreGive(parametros->Semáforo_min);
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_min);
        }
    }
}

void Actualizar_pantalla_dec (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Decena = 0;
    while(1){
            xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_dec,  
            pdTRUE,         
            pdFALSE,
            portMAX_DELAY);
            xSemaphoreTake(parametros->Semáforo_dec,portMAX_DELAY);
            Decena = Decimas%10;
            xSemaphoreGive(parametros->Semáforo_dec);
            xSemaphoreTake(parametros->Semaforo_UART,portMAX_DELAY);
            DibujarDigito(parametros->Panel_dec,0,Decena);
            xSemaphoreGive(parametros->Semaforo_UART);

    }
}

void Actualizar_pantalla_seg (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    while(1){
        xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_seg,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
            xSemaphoreTake(parametros->Semáforo_seg,portMAX_DELAY);
            Unidad = Segundos%10;
            Decena = Segundos/10;
            xSemaphoreGive(parametros->Semáforo_seg);
            xSemaphoreTake(parametros->Semaforo_UART,portMAX_DELAY);
            DibujarDigito(parametros->Panel_seg,0,Decena);
            DibujarDigito(parametros->Panel_seg,1,Unidad);
            xSemaphoreGive(parametros->Semaforo_UART);
        
    }
}

void Actualizar_pantalla_min (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    while(1){
        xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_min,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
            xSemaphoreTake(parametros->Semáforo_min,portMAX_DELAY);
            Decena = Minutos/10;
            Unidad = Minutos%10;
            xSemaphoreGive(parametros->Semáforo_min);
            xSemaphoreTake(parametros->Semaforo_UART,portMAX_DELAY);
            DibujarDigito(parametros->Panel_min,0,Decena);
            DibujarDigito(parametros->Panel_min,1,Unidad);
            xSemaphoreGive(parametros->Semaforo_UART);
        
    }
}

void app_main (void){
    SemaphoreHandle_t  Semaforo_Decimas; 
    SemaphoreHandle_t  Semaforo_Segundos; 
    SemaphoreHandle_t  Semaforo_Minutos; 
    SemaphoreHandle_t  Semaforo_Estado_Cronometro;
    SemaphoreHandle_t  Semaforo_Conteo_Global;
    SemaphoreHandle_t  Periférico_UART;

    Semaforo_Decimas                =xSemaphoreCreateMutex();
    Semaforo_Segundos               =xSemaphoreCreateMutex();
    Semaforo_Minutos                =xSemaphoreCreateMutex();
    Semaforo_Estado_Cronometro      =xSemaphoreCreateMutex();
    Semaforo_Conteo_Global          =xSemaphoreCreateMutex();
    Periférico_UART                 =xSemaphoreCreateMutex();

    EventGroupHandle_t Grupo_eventos;
    Grupo_eventos = xEventGroupCreate();
    static struct Cronometro_s Control_temporal;
    
    Control_temporal.Eventos_task=Grupo_eventos;
    Control_temporal.Semaforo_Estado_G=Semaforo_Estado_Cronometro;
    Control_temporal.Semaforo_UART=Periférico_UART;
    Control_temporal.Semáforo_dec=Semaforo_Decimas;
    Control_temporal.Semáforo_seg=Semaforo_Segundos;
    Control_temporal.Semáforo_min=Semaforo_Minutos;
    Control_temporal.Semáforo_variable=Semaforo_Conteo_Global;
    
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    Control_temporal.Panel_dec=CrearPanel(240, 10, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_seg=CrearPanel(140, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_min=CrearPanel(40, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
   
    DibujarDigito(Control_temporal.Panel_dec, 0, 0);
    DibujarDigito(Control_temporal.Panel_seg, 0, 0);
    DibujarDigito(Control_temporal.Panel_seg, 1, 0);
    DibujarDigito(Control_temporal.Panel_min, 0, 0);
    DibujarDigito(Control_temporal.Panel_min, 1, 0);
    ILI9341DrawFilledCircle(130, 80, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(230, 80, 5, DIGITO_ENCENDIDO);
    

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


    xTaskCreate(Parpadeo_led_verde,NULL,6*configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Testeo_Botones,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Start_Stop_Cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Reset_Cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+1,NULL);
    xTaskCreate(Contador,"Counter_dec",6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+4,NULL);
    xTaskCreate(Actualizar_valores,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    xTaskCreate(Actualizar_pantalla_dec,"Actualizar_pantalla_dec",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Actualizar_pantalla_seg,"Actualizar_pantalla_seg",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Actualizar_pantalla_min,"Actualizar_pantalla_min",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);

}