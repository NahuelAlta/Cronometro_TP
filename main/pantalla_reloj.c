#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"
#include "pantalla_reloj.h"
#include "definiciones.h"


void Actualizar_pantalla_min_reloj (void *parameters){
    Reloj_t parametros = (Reloj_t) parameters;
    static uint32_t Unidad = 0;
    static uint32_t Decena = 0;
    static uint32_t Decena_anterior = 0;
    static uint32_t Conteo_local = 0;
    static uint32_t Minutos_local = 0;
    EventBits_t Bits_evento;
    while(1){
            Bits_evento=xEventGroupWaitBits(parametros->Eventos_reloj,
                                    Pantalla_min_r | Todo_min_r,
                                    pdTRUE,
                                    pdFALSE,
                                    portMAX_DELAY);
            if ((Bits_evento & Pantalla_min_r)!=0){
                xQueueReceive(parametros->Variable_Reloj,(void *)&Conteo_local,portMAX_DELAY);
                Minutos_local=Conteo_local%60;
                Bits_evento=xEventGroupGetBits(parametros->Eventos_reloj);
                if ((Bits_evento & (Pantalla_modo_hora | Seteando_hora | Seteando_alarma))!=0){
                    xSemaphoreTake(parametros->Semaforo_SPI_Reloj,portMAX_DELAY);
                    if (!Minutos_local){
                        DibujarDigito(parametros->Panel_min_reloj,0,0);
                        DibujarDigito(parametros->Panel_min_reloj,1,0);
                        xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                        Decena_anterior=0;
                    }
                    else{
                        Unidad = Minutos_local%10;
                        Decena = Minutos_local/10;
                        if (Decena != Decena_anterior){
                            DibujarDigito(parametros->Panel_min_reloj,0,Decena);
                            Decena_anterior = Decena;
                        }
                        DibujarDigito(parametros->Panel_min_reloj,1,Unidad);
                        xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                    }
                }
            }
            else{
                xQueueReceive(parametros->Variable_Reloj,(void *)&Conteo_local,portMAX_DELAY);
                Minutos_local=Conteo_local%60;
                Unidad = Minutos_local%10;
                Decena = Minutos_local/10;                
                xSemaphoreTake(parametros->Semaforo_SPI_Reloj,portMAX_DELAY);
                DibujarDigito(parametros->Panel_min_reloj,0,Decena);
                DibujarDigito(parametros->Panel_min_reloj,1,Unidad);
                xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                xEventGroupSetBits(parametros->Eventos_reloj,Digitos_seteados_M);
            }
            
    }
}

void Actualizar_pantalla_hora_reloj (void *parameters){
    Reloj_t parametros = (Reloj_t) parameters;
    static uint32_t Unidad = 0;
    static uint32_t Decena = 0;
    static uint32_t Decena_anterior = 0;
    static uint32_t Conteo_local = 0;
    static uint32_t Hora_local = 0;
    EventBits_t Bits_evento;
    while(1){
            Bits_evento=xEventGroupWaitBits(parametros->Eventos_reloj,
                                    Pantalla_hora_r | Todo_hora_r,
                                    pdTRUE,
                                    pdFALSE,
                                    portMAX_DELAY);
            if ((Bits_evento & Pantalla_hora_r) !=0){                                    
                xQueueReceive(parametros->Variable_Reloj_hora,(void *)&Conteo_local,portMAX_DELAY);
                Hora_local=(Conteo_local/60)%24;
                Bits_evento=xEventGroupGetBits(parametros->Eventos_reloj);
                if ((Bits_evento & (Pantalla_modo_hora| Seteando_hora | Seteando_alarma))!=0){
                    xSemaphoreTake(parametros->Semaforo_SPI_Reloj,portMAX_DELAY);
                    if (!Hora_local){
                        DibujarDigito(parametros->Panel_hora_reloj,0,0);
                        DibujarDigito(parametros->Panel_hora_reloj,1,0);
                        xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                        Decena_anterior=0;
                    }
                    else{
                        Unidad = Hora_local%10;
                        Decena = Hora_local/10;
                        if (Decena != Decena_anterior){
                            DibujarDigito(parametros->Panel_hora_reloj,0,Decena);
                            Decena_anterior = Decena;
                        }
                        DibujarDigito(parametros->Panel_hora_reloj,1,Unidad);
                        xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                    }
                }
            }
            else{
                xQueueReceive(parametros->Variable_Reloj_hora,(void *)&Conteo_local,portMAX_DELAY);
                Hora_local=(Conteo_local/60)%24;
                Unidad = Hora_local%10;
                Decena = Hora_local/10;
                Decena_anterior = Decena;                
                xSemaphoreTake(parametros->Semaforo_SPI_Reloj,portMAX_DELAY);
                DibujarDigito(parametros->Panel_hora_reloj,0,Decena);
                DibujarDigito(parametros->Panel_hora_reloj,1,Unidad);
                xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
                xEventGroupSetBits(parametros->Eventos_reloj,Digitos_seteados_H);
            }
    }
}

void Estado_configuracion (void* parameters){ //indicaciones visuales
    Reloj_t Evento = (Reloj_t) parameters;
    EventBits_t Bits_Eventos;
    static uint8_t Parpadeo_alarma=0;
    static uint8_t Parpadeo_hora=0;
    while(1){
        Bits_Eventos=xEventGroupWaitBits(Evento->Eventos_reloj,
            Seteando_alarma | Seteando_hora | Pantalla_modo_hora, //Eventos_control
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
        if ((Bits_Eventos & Seteando_alarma)!=0){
            if (!Parpadeo_alarma){
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawString(8,140,"CONFIGURANDO ALARMA",&font_16x26,ILI9341_BLUE,DIGITO_APAGADO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_alarma^=1;
            }
            else{
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawString(8,140,"CONFIGURANDO ALARMA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_alarma^=1;
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else if (((Bits_Eventos & Seteando_hora)!=0)){
            if (!Parpadeo_hora){
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawString(24,140,"CONFIGURANDO HORA",&font_16x26,ILI9341_GREEN,DIGITO_APAGADO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_hora^=1;
            }
            else{
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawString(24,140,"CONFIGURANDO HORA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_hora^=1;
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else if (((Bits_Eventos & Pantalla_modo_hora)!=0)) {
            if (!Parpadeo_hora){
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawFilledCircle(160, 20, 5, DIGITO_ENCENDIDO);
                ILI9341DrawFilledCircle(160, 100, 5, DIGITO_ENCENDIDO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_hora^=1;
            }
            else{
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawFilledCircle(160, 20, 5, DIGITO_APAGADO);
                ILI9341DrawFilledCircle(160, 100, 5, DIGITO_APAGADO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Parpadeo_hora^=1;
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void Refrescar_hora_pantalla (void* param){
    Reloj_t Evento = (Reloj_t) param;
    EventBits_t Bits_evento;
    static uint32_t conteo_backup=0;
    while(1){
        Bits_evento=xEventGroupWaitBits(Evento->Eventos_reloj,
                        Actualizar_pantalla,
                        pdTRUE,
                        pdTRUE,
                        portMAX_DELAY);
        xQueueReceive(Evento->Variable_Reloj_Actual,(void*)&conteo_backup,portMAX_DELAY);
        xQueueSend(Evento->Variable_Reloj,(void*)&conteo_backup,portMAX_DELAY);
        xQueueSend(Evento->Variable_Reloj_hora,(void*)&conteo_backup, portMAX_DELAY);
        if ((Bits_evento & Pantalla_modo_hora)==0){
            xEventGroupSetBits(Evento->Eventos_reloj,Todo_hora_r|Todo_min_r);
            xEventGroupWaitBits(Evento->Eventos_reloj,
                        Digitos_seteados_M | Digitos_seteados_H,
                        pdTRUE,
                        pdTRUE,
                        portMAX_DELAY);
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_modo_hora); //Eventos_Control
        }
        else{
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_hora_r|Pantalla_min_r);
        }
        if ((Bits_evento & Alarma_estado)!=0){
            xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
            ILI9341DrawString(48,140,"ALARMA SETEADA",&font_16x26,ILI9341_BLUE,DIGITO_APAGADO);
            xSemaphoreGive(Evento->Semaforo_SPI_Reloj);   
        }
           
    }
}

void Alarma_notificacion (void * parameters){
    Reloj_t parametros = (Reloj_t) parameters;
    uint32_t Valor_alarma = 0;
    uint32_t Valor_hora = 0;
    EventBits_t Bits_evento;
    while(1){
        Bits_evento = xEventGroupWaitBits(parametros->Eventos_reloj,
                            (Alarma_estado | Seteando_alarma),
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        if ((Bits_evento & Alarma_estado)!=0){
            if ((Bits_evento & Seteando_alarma)!=0){
                xQueueReceive(parametros->Variable_Reloj_Alarma, (void*)&Valor_alarma, portMAX_DELAY); 
                Valor_alarma=Valor_alarma%1440;
            }
            else{
                xQueueReceive(parametros->Variable_Reloj_Alarma, (void*)&Valor_hora, portMAX_DELAY);
                Valor_hora=Valor_hora%1440;
                if (Valor_hora == Valor_alarma){
                xEventGroupSetBits(parametros->Eventos_reloj, Alarma_sonando);
                }
            }
        }
        else if ((Bits_evento & Seteando_alarma)!=0){
            xQueueReceive(parametros->Variable_Reloj_Alarma, (void*)&Valor_alarma, portMAX_DELAY); 
            Valor_alarma=Valor_alarma%1440;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void Alarma_evento (void * parameters){
    Reloj_t parametros = (Reloj_t) parameters;
    uint8_t Estado=0;
    uint16_t Color;
    while(1){
            xEventGroupWaitBits(parametros->Eventos_reloj,
                            Alarma_sonando,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
            if(!Estado){
                Color = ILI9341_RED;
            }
            else{
                Color = ILI9341_BLACK;
            }
            xSemaphoreTake(parametros->Semaforo_SPI_Reloj,portMAX_DELAY);
            ILI9341DrawFilledRectangle(0,0,10,240,Color);
            ILI9341DrawFilledRectangle(0,240,320,230,Color);
            ILI9341DrawFilledRectangle(320,240,310,0,Color);
            ILI9341DrawFilledRectangle(320,0,0,10,Color);
            xSemaphoreGive(parametros->Semaforo_SPI_Reloj);
            Estado^=1;
            vTaskDelay(250 / portTICK_PERIOD_MS);
            }
        }