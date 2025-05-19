#include "cronometro.h"
#include "definiciones.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"

void Start_Stop_Cronometro (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    EventBits_t Bits_return = 0;
    static uint8_t Estado_Global=PARADO;
    while(1){
        Bits_return = xEventGroupWaitBits(parametros->Eventos_task,
                                            Interrupcion_TC0,
                                            pdTRUE,
                                            pdFALSE,
                                            portMAX_DELAY);
        if ((Bits_return & Interrupcion_TC0) == Interrupcion_TC0) {
            if (Estado_Global == PARADO){
                Estado_Global = CORRIENDO;
                xEventGroupSetBits(parametros->Eventos_task,Cronometro_estado);
            }
            else{
                Estado_Global = PARADO;
                xEventGroupClearBits(parametros->Eventos_task,Cronometro_estado);
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                ILI9341DrawFilledCircle(20,45,8,ILI9341_RED);
                ILI9341DrawFilledCircle(300,45,8,ILI9341_RED);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
        }
    }
}

void Contador (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    TickType_t Tiempo_inicial = 0;
    uint32_t Conteo_local=0;
    Tiempo_inicial = xTaskGetTickCount();
    static EventBits_t Bits_return;
    while(1){
        Bits_return=xEventGroupWaitBits(parametros->Eventos_task,
                            Cronometro_estado | Resetear_Ev | Interrupcion_TC2,
                            pdFALSE,
                            pdFALSE,
                            0);
        if ((Bits_return & Cronometro_estado)!=0){
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
            Conteo_local+=1;
            xQueueSend(parametros->Variable_conteo,(void*)&Conteo_local,0);
            xQueueOverwrite(parametros->Variable_conteo_Backup,(void*)&Conteo_local);
        }
        else if((Bits_return & Resetear_Ev)!=0) {
            Conteo_local=0;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            ILI9341Fill(ILI9341_BLACK);
            xSemaphoreGive(parametros->Semaforo_SPI);
            xEventGroupSetBits(parametros->Eventos_task, Resetear_laps);
            xEventGroupClearBits(parametros->Eventos_task,Resetear_Ev);
            xQueueSend(parametros->Variable_conteo,(void*)&Conteo_local,portMAX_DELAY);
            xQueueOverwrite(parametros->Variable_conteo_Backup,(void*)&Conteo_local);
        }
        else{
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(100));
        }
        if ((Bits_return & Interrupcion_TC2)!=0){
                xQueueSend(parametros->Cola_lap,(void*)&Conteo_local,portMAX_DELAY);
                xEventGroupClearBits(parametros->Eventos_task,Interrupcion_TC2);
            }
        
       
    }
}

void Actualizar_valores(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint32_t Conteo_local = 0;
    static uint32_t Decimas=0;
    static uint32_t Segundos=0;
    static uint32_t Minutos=0;
    EventBits_t Bits_evento;
    while(1){
        Bits_evento=xEventGroupWaitBits(parametros->Eventos_task,
                            Cronometro_estado | Resetear_Ev,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        xQueueReceive(parametros->Variable_conteo,(void*)&Conteo_local,portMAX_DELAY);
        Decimas = Conteo_local%10;
        if((Bits_evento & Pantalla_modo_cronometro)!=0){
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_dec);
            xQueueSend(parametros->Queue_dec,(void *)&Decimas,portMAX_DELAY);
            if ((Conteo_local%10)==0){
                Segundos = (Conteo_local/10)%60;
                xEventGroupSetBits(parametros->Eventos_task,Pantalla_seg);
                xQueueSend(parametros->Queue_seg,(void *)&Segundos,portMAX_DELAY);
            }
            if ((Conteo_local%600)==0){
                xEventGroupSetBits(parametros->Eventos_task,Pantalla_min);
                Minutos = Conteo_local/600;
                xQueueSend(parametros->Queue_min,(void *)&Minutos,portMAX_DELAY);
            }
            if (!Conteo_local && ((Bits_evento & Pantalla_modo_cronometro)!=0)){
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                ILI9341DrawFilledCircle(130, 80, 5, DIGITO_ENCENDIDO);
                ILI9341DrawFilledCircle(230, 80, 5, DIGITO_ENCENDIDO);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
        } 
        
    }
}






