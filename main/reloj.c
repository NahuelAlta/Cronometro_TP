#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"
#include "reloj.h"
#include "definiciones.h"

void Reloj_cuenta (void *parameters){
    Reloj_t parametros = (Reloj_t) parameters;
    TickType_t Tiempo_inicial = 0;
    Tiempo_inicial = xTaskGetTickCount();
    static uint32_t Conteo_local=0;
    static EventBits_t Bits_return;
    while(1){
        Bits_return=xEventGroupWaitBits(parametros->Eventos_reloj,
                            Reloj_estado | Seteando_hora,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        if ((Bits_return & Reloj_estado)!=0){
            xQueueOverwrite(parametros->Variable_Reloj_Actual,(void*)&Conteo_local);
            vTaskDelayUntil(&Tiempo_inicial,pdMS_TO_TICKS(1500));
            Conteo_local+=1;
            Bits_return=xEventGroupGetBits(parametros->Eventos_reloj);
            if((Bits_return & (Seteando_alarma | Seteando_hora))==0){ //Si NO esta configurando, 
                xEventGroupSetBits(parametros->Eventos_reloj, Pantalla_min_r);
                xQueueSend(parametros->Variable_Reloj,(void*)&Conteo_local,portMAX_DELAY); //Para pantalla min
                if (!(Conteo_local%60)){
                    xEventGroupSetBits(parametros->Eventos_reloj, Pantalla_hora_r);
                    xQueueSend(parametros->Variable_Reloj_hora,(void*)&Conteo_local,portMAX_DELAY);
                }
                if ((Bits_return & Alarma_estado)!=0){
                    xQueueSend(parametros->Variable_Reloj_Alarma,(void*)&Conteo_local,portMAX_DELAY);//Envía un dato a la tarea encargada de comparar 
                }
            } 
        }
        else if ((Bits_return & Seteando_hora)!=0){
            xQueueReceive(parametros->Variable_Reloj_nueva,(void*)&Conteo_local,portMAX_DELAY);
            Tiempo_inicial = xTaskGetTickCount();
        }       
    }
}