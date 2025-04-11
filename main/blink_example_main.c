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

#define DIGITO_ANCHO     30
#define DIGITO_ALTO      100
#define DECIMA_ANCHO     30
#define DECIMA_ALTO      80
#define LAP_ANCHO        20
#define LAP_ALTO         50
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO   0x3800
#define DIGITO_FONDO     ILI9341_BLACK

uint32_t Minutos = 0;
uint32_t Decimas = 0;
uint32_t Segundos = 0;

uint8_t Estado_Global=0;

typedef struct Cronometro_s {
    uint8_t Tope;
    uint32_t *puntero_variable; //Va a controlar el acceso a la variable de Conteo
    uint32_t Delay;
    uint32_t Bits_de_cambio;
    SemaphoreHandle_t Semáforo_variable; //Va a controlar el acceso a la variable de Estado_global
    SemaphoreHandle_t Estado_Cronometro;
    SemaphoreHandle_t Semaforo_UART;
    EventGroupHandle_t Eventos_task;
    panel_t Panel_grupo;
    
}* Cronometro_t;

typedef struct Pantalla_Lap_s {
    panel_t Panel_lap1;
    panel_t Panel_lap2;
    panel_t Panel_lap3;
    panel_t Panel_lap4;
} *Pantalla_Lap_t;

typedef struct Control_teclas_cronometro_s {
    EventGroupHandle_t  Evento;
    SemaphoreHandle_t   Estado_Cronometro;
    SemaphoreHandle_t   Semaforo_segundos;
    SemaphoreHandle_t   Semaforo_minutos;
    SemaphoreHandle_t   Semaforo_decimas;
} *Control_teclas_t;

void Handler_interrupcion_pin_TC0(void *args){
    EventGroupHandle_t Evento = (EventGroupHandle_t) args;
    xEventGroupSetBitsFromISR(Evento,Interrupcion_TC0,pdFALSE);
}
void Handler_interrupcion_pin_TC1(void *args){
    EventGroupHandle_t Evento = (EventGroupHandle_t) args;
    xEventGroupSetBitsFromISR(Evento,Interrupcion_TC1,pdFALSE);
}
void Handler_interrupcion_pin_TC2(void *args){
    EventGroupHandle_t Evento = (EventGroupHandle_t) args;
    xEventGroupSetBitsFromISR(Evento,Interrupcion_TC2,pdFALSE);
}
static void Parpadeo_led_verde (void *parameters){
    SemaphoreHandle_t Estado_G = (SemaphoreHandle_t) parameters;
    uint32_t Estado_local=0;
    static uint8_t estado=1;
    while(1){
        xSemaphoreTake(Estado_G,portMAX_DELAY);
        Estado_local = Estado_Global;
        xSemaphoreGive(Estado_G);
        if (Estado_G){
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

}
static void Reset_Cronometro (void *parameters){
    
}
static void Obtener_Lap (void *parameters){
    
}
static void Contador (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static TickType_t Tiempo_inicial = 0;
    Tiempo_inicial = xTaskGetTickCount();
    while(1){
        xSemaphoreTake(parametros->Estado_Cronometro,portMAX_DELAY);
        if (Estado_Global){
            xSemaphoreGive(parametros->Estado_Cronometro);
            xSemaphoreTake(parametros->Semáforo_variable,portMAX_DELAY);
            *(parametros->puntero_variable)+=1;
            if (parametros->Tope == *(parametros->puntero_variable)){
                *(parametros->puntero_variable)=0;
            }
            xSemaphoreGive(parametros->Semáforo_variable);
            xEventGroupSetBits(parametros->Eventos_task,parametros->Bits_de_cambio);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(parametros->Delay));
        }
        else{
            xSemaphoreGive(parametros->Estado_Cronometro);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
        }
    }
}

static void Actualizar_pantalla(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    EventBits_t Bits_evento;
    uint8_t Decena=0;
    uint8_t Unidad=0;
    while(1){
        Bits_evento = xEventGroupWaitBits(
            parametros->Eventos_task,    
            parametros->Bits_de_cambio,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
        xSemaphoreTake(parametros->Semáforo_variable,portMAX_DELAY);
        Decena=*(parametros->puntero_variable)/10;
        Unidad=*(parametros->puntero_variable)%10;
        xSemaphoreGive(parametros->Semáforo_variable);
        xSemaphoreTake(parametros->Semaforo_UART,portMAX_DELAY);
        DibujarDigito(parametros->Panel_grupo, 0, Decena);
        DibujarDigito(parametros->Panel_grupo, 1, Unidad);
        xSemaphoreGive(parametros->Semaforo_UART);
        }
}

void app_main (void){
    SemaphoreHandle_t  Semaforo_Decimas; 
    SemaphoreHandle_t  Semaforo_Segundos; 
    SemaphoreHandle_t  Semaforo_Minutos; 
    SemaphoreHandle_t  Semaforo_Estado_Cronometro;
    SemaphoreHandle_t  Periférico_UART;

    Semaforo_Decimas                =xSemaphoreCreateMutex();
    Semaforo_Segundos               =xSemaphoreCreateMutex();
    Semaforo_Minutos                =xSemaphoreCreateMutex();
    Semaforo_Estado_Cronometro      =xSemaphoreCreateMutex();
    Periférico_UART                 =xSemaphoreCreateMutex();

    EventGroupHandle_t Grupo_eventos;
    Grupo_eventos = xEventGroupCreate();

    static struct Cronometro_s Control_temporal[]={
        {.Bits_de_cambio = Cambio_dec, .Delay=100, .puntero_variable=&Decimas, .Tope=10},
        {.Bits_de_cambio = Cambio_min, .Delay=60000, .puntero_variable=&Minutos, .Tope=60},
        {.Bits_de_cambio = Cambio_seg, .Delay=1000, .puntero_variable=&Segundos, .Tope=60}
    };  
    Control_temporal[0].Estado_Cronometro=Semaforo_Estado_Cronometro;
    Control_temporal[1].Estado_Cronometro=Semaforo_Estado_Cronometro;
    Control_temporal[2].Estado_Cronometro=Semaforo_Estado_Cronometro;

    Control_temporal[0].Semáforo_variable=Semaforo_Decimas;
    Control_temporal[1].Semáforo_variable=Semaforo_Minutos;
    Control_temporal[2].Semáforo_variable=Semaforo_Segundos;

    Control_temporal[0].Semaforo_UART=Periférico_UART;
    Control_temporal[1].Semaforo_UART=Periférico_UART;
    Control_temporal[2].Semaforo_UART=Periférico_UART;

    Control_temporal[0].Eventos_task=Grupo_eventos;
    Control_temporal[1].Eventos_task=Grupo_eventos;
    Control_temporal[2].Eventos_task=Grupo_eventos;
    
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    Control_temporal[0].Panel_grupo=CrearPanel(50, 125, 2, DECIMA_ALTO, DECIMA_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal[1].Panel_grupo=CrearPanel(10, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal[2].Panel_grupo=CrearPanel(90, 10, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
   
    DibujarDigito(Control_temporal[0].Panel_grupo, 0, 0);
    DibujarDigito(Control_temporal[0].Panel_grupo, 1, 0);
    DibujarDigito(Control_temporal[1].Panel_grupo, 0, 0);
    DibujarDigito(Control_temporal[1].Panel_grupo, 1, 0);
    DibujarDigito(Control_temporal[2].Panel_grupo, 0, 0);
    DibujarDigito(Control_temporal[2].Panel_grupo, 1, 0);

    static struct Control_teclas_cronometro_s Teclas_control;
    Teclas_control.Estado_Cronometro=Semaforo_Estado_Cronometro;
    Teclas_control.Evento=Grupo_eventos;
    Teclas_control.Semaforo_decimas=Semaforo_Decimas;
    Teclas_control.Semaforo_segundos=Semaforo_Segundos;
    Teclas_control.Semaforo_minutos=Semaforo_Minutos;
    
    ILI9341DrawFilledCircle(75, 105, 5, DIGITO_ENCENDIDO);

    gpio_config_t io_conf_int = {};
    io_conf_int.pin_bit_mask =
        ((1ULL << BOTON_TC0) | (1ULL << BOTON_TC1) | (1ULL << BOTON_TC2));
    io_conf_int.mode = GPIO_MODE_INPUT;
    io_conf_int.pull_up_en = true;
    io_conf_int.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&io_conf_int);

    gpio_config_t io_conf_int2 = {};
    io_conf_int2.pin_bit_mask =
        ((1ULL << LED_ROJO) | (1ULL << LED_VERDE));
    io_conf_int2.mode = GPIO_MODE_OUTPUT;
    io_conf_int2.pull_up_en = false;
    io_conf_int2.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int2);


    xTaskCreate(Parpadeo_led_verde,NULL,2*configMINIMAL_STACK_SIZE,(void*)Semaforo_Estado_Cronometro,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Start_Stop_Cronometro,NULL,3*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Reset_Cronometro,NULL,3*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 1,NULL);
    xTaskCreate(Contador,"Counter_dec",configMINIMAL_STACK_SIZE,(void*)&Control_temporal[0],tskIDLE_PRIORITY + 3,NULL);
    xTaskCreate(Contador,"Counter_seg",configMINIMAL_STACK_SIZE,(void*)&Control_temporal[2],tskIDLE_PRIORITY + 3,NULL);
    xTaskCreate(Contador,"Counter_min",configMINIMAL_STACK_SIZE,(void*)&Control_temporal[1],tskIDLE_PRIORITY + 3,NULL);

    
    xTaskCreate(Actualizar_pantalla,"Actualizar_pantalla_dec",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY + 2,NULL);

    }