#include "cronometro.h"
#include "definiciones.h"
#include "pantalla_cronometro.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"

void Mostrar_Lap (void *parameters){
    Pantalla_Lap_t Estructura = (Pantalla_Lap_t) parameters;
    static uint32_t minutos=0;
    static uint32_t segundos=0;
    static uint32_t decimas=0;
    static uint32_t panel_n = 0;
    static uint32_t Buffer_dato[2] = {0};
    static uint8_t Muestras=0;
    EventBits_t Bits_Eventos;
    while(1){
            Bits_Eventos=xEventGroupWaitBits(
                   Estructura->Eventos_task,    
                   Refrescar_lap | Interrupcion_TC2 | Resetear_laps,  
                   pdFALSE,         
                   pdFALSE,        
                   portMAX_DELAY);
            if ((Bits_Eventos & Interrupcion_TC2)!=0){
                xQueueReceive(Estructura->Cola_lap, &Buffer_dato[panel_n], portMAX_DELAY);
                decimas = Buffer_dato[panel_n]%10;
                segundos = (Buffer_dato[panel_n]/10)%60;
                minutos = Buffer_dato[panel_n]/600;
                if (Muestras!=2){
                    Muestras +=1;
                }
                if (!panel_n){
                    xSemaphoreTake(Estructura->Semaforo_SPI,portMAX_DELAY);
                    ILI9341DrawFilledCircle(135,156,2,DIGITO_ENCENDIDO);
                    ILI9341DrawFilledCircle(195,156,2,DIGITO_ENCENDIDO);
                }
                else{
                    xSemaphoreTake(Estructura->Semaforo_SPI,portMAX_DELAY);
                    ILI9341DrawFilledCircle(135,226,2,DIGITO_ENCENDIDO);
                    ILI9341DrawFilledCircle(195,226,2,DIGITO_ENCENDIDO);
                }
                DibujarDigito(Estructura->Paneles[panel_n].Decimas_panel,0,decimas);
                DibujarDigito(Estructura->Paneles[panel_n].Segundos_panel,0,segundos/10);
                DibujarDigito(Estructura->Paneles[panel_n].Segundos_panel,1,segundos%10);
                DibujarDigito(Estructura->Paneles[panel_n].Minutos_panel,0,minutos/10);
                DibujarDigito(Estructura->Paneles[panel_n].Minutos_panel,1,minutos%10);
                xSemaphoreGive(Estructura->Semaforo_SPI);
                panel_n^=1;
            }
            else if ((Bits_Eventos & Refrescar_lap)!=0){
                xEventGroupClearBits(Estructura->Eventos_task,Refrescar_lap);
                if (Muestras){
                    xSemaphoreTake(Estructura->Semaforo_SPI,portMAX_DELAY);                       
                    for(uint8_t posicion=0;posicion<=(Muestras-1);posicion++){
                        ILI9341DrawFilledCircle(135,156+70*(posicion),2,DIGITO_ENCENDIDO);
                        ILI9341DrawFilledCircle(195,156+70*(posicion),2,DIGITO_ENCENDIDO);
                        decimas = Buffer_dato[posicion]%10;
                        segundos = (Buffer_dato[posicion]/10)%60;
                        minutos = Buffer_dato[posicion]/600;
                        DibujarDigito(Estructura->Paneles[posicion].Decimas_panel,0,decimas);
                        DibujarDigito(Estructura->Paneles[posicion].Segundos_panel,0,segundos/10);
                        DibujarDigito(Estructura->Paneles[posicion].Segundos_panel,1,segundos%10);
                        DibujarDigito(Estructura->Paneles[posicion].Minutos_panel,0,minutos/10);
                        DibujarDigito(Estructura->Paneles[posicion].Minutos_panel,1,minutos%10);
                    }
                    panel_n=0;
                    xSemaphoreGive(Estructura->Semaforo_SPI);
                }
            }
            else{
                xEventGroupClearBits(Estructura->Eventos_task,Resetear_laps);
                Buffer_dato[0]=0;
                Buffer_dato[1]=0;
                panel_n=0;
                Muestras=0;
            }
        }         
    }

void Actualizar_pantalla_seg (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    static uint8_t Decena_anterior = 0;
    static uint32_t Segundos_local = 0;
    EventBits_t Bits_evento;
    while(1){
            Bits_evento=xEventGroupWaitBits(parametros->Eventos_task,
                                    Pantalla_seg | Todo_seg_cro,
                                    pdTRUE,
                                    pdFALSE,
                                    portMAX_DELAY);
            xQueueReceive(parametros->Queue_seg,(void *)&Segundos_local,portMAX_DELAY); 
            if ((Bits_evento & Pantalla_seg)!=0){
                if (!Segundos_local){
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                DibujarDigito(parametros->Panel_seg,0,0);
                DibujarDigito(parametros->Panel_seg,1,0);
                xSemaphoreGive(parametros->Semaforo_SPI);
                }
                else{
                    Unidad = Segundos_local%10;
                    Decena = Segundos_local/10;
                    if ((Bits_evento & Pantalla_modo_cronometro)!=0){
                        xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                        if (Decena != Decena_anterior){
                            DibujarDigito(parametros->Panel_seg,0,Decena);
                            Decena_anterior = Decena;
                        }
                        DibujarDigito(parametros->Panel_seg,1,Unidad);
                        xSemaphoreGive(parametros->Semaforo_SPI);
                    }
                }
            }
            else{
                Unidad = Segundos_local%10;
                Decena = Segundos_local/10;
                Decena_anterior = Decena;
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                DibujarDigito(parametros->Panel_seg,0,Decena);
                DibujarDigito(parametros->Panel_seg,1,Unidad);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
                    
        }
}

void Actualizar_pantalla_min (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    static uint8_t Decena_anterior = 0;
    static uint32_t Minutos_local = 0;
    EventBits_t Bits_evento;
    while(1){
        Bits_evento=xEventGroupWaitBits(parametros->Eventos_task,
                                    Pantalla_min | Todo_min_cro,
                                    pdTRUE,
                                    pdFALSE,
                                    portMAX_DELAY);
        if ((Bits_evento & Pantalla_min)!=0){
            xQueueReceive(parametros->Queue_min,(void *)&Minutos_local,portMAX_DELAY);
            if (!Minutos_local){
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                DibujarDigito(parametros->Panel_min,0,0);
                DibujarDigito(parametros->Panel_min,1,0);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
            else{
                Decena = Minutos_local/10;
                Unidad = Minutos_local%10;
                if ((Bits_evento & Pantalla_modo_cronometro)!=0)
                {
                    xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                    if ((Decena != Decena_anterior)){
                        DibujarDigito(parametros->Panel_min,0,Decena);
                        Decena_anterior = Decena;
                    }
                    DibujarDigito(parametros->Panel_min,1,Unidad);
                    xSemaphoreGive(parametros->Semaforo_SPI);
                }
                
            }
        }
        else{
            xQueueReceive(parametros->Queue_min,(void *)&Minutos_local,portMAX_DELAY);
            Decena = Minutos_local/10;
            Unidad = Minutos_local%10;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            DibujarDigito(parametros->Panel_min,0,Decena);
            DibujarDigito(parametros->Panel_min,1,Unidad);
            xSemaphoreGive(parametros->Semaforo_SPI);
            Decena_anterior = Decena;
        }        
    }
}

void Actualizar_pantalla_dec (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Decena = 0;
    static uint32_t Decimas_local=0;
    EventBits_t Bits_evento;
    while(1){
            Bits_evento=xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_dec,  //Falta cuando no está el cronometro encendido
            pdTRUE,         
            pdTRUE,
            portMAX_DELAY);
            xQueueReceive(parametros->Queue_dec,(void *)&Decimas_local,portMAX_DELAY);
            Decena = Decimas_local%10;
            if ((Bits_evento & Pantalla_modo_cronometro)!=0){
                xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
                DibujarDigito(parametros->Panel_dec,0,Decena);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
            
    }
}

void Parpadeo_led_verde (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t estado=1;
    EventBits_t Bits_return;
    while(1){
        Bits_return=xEventGroupWaitBits(parametros->Eventos_task,
                            Cronometro_estado | Pantalla_modo_cronometro,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);
        if (((Bits_return)&(Cronometro_estado)) == (Cronometro_estado)){
            estado^=1;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            if (estado){
                ILI9341DrawFilledCircle(20,45,8,ILI9341_GREEN);
                ILI9341DrawFilledCircle(300,45,8,ILI9341_GREEN);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
            else{
                ILI9341DrawFilledCircle(20,45,8,DIGITO_APAGADO);
                ILI9341DrawFilledCircle(300,45,8,DIGITO_APAGADO);
                xSemaphoreGive(parametros->Semaforo_SPI);
            }
            vTaskDelay(250/portTICK_PERIOD_MS);
            }
        else{
            vTaskDelay(250/portTICK_PERIOD_MS);
        }
        
    }
}
