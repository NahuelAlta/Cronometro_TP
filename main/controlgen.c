#include "definiciones.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"
#include "controlgen.h"

void Testeo_Botones_cronometro(void *parameters){
    Cronometro_t Evento = (Cronometro_t) parameters;
    EventBits_t Bits_return;
    while(1){
        xEventGroupWaitBits(Evento->Eventos_task, //Eventos_control
                            Pantalla_modo_cronometro, 
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
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
            Bits_return=xEventGroupWaitBits(Evento->Eventos_task,  // Eventos_control
                            Cronometro_estado,
                            pdFALSE,
                            pdFALSE,
                            0);
            if ((Bits_return & Cronometro_estado)==0){
                xEventGroupSetBits(Evento->Eventos_task,Resetear_Ev);
            }
        }
        else if (!gpio_get_level(BOTON_TC2))
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC2)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupSetBits(Evento->Eventos_task,Interrupcion_TC2);
        }
        else if (!gpio_get_level(BOTON_MODO)){
            vTaskDelay(200 / portTICK_PERIOD_MS);
            if (!gpio_get_level(BOTON_MODO)){
                while(!gpio_get_level(BOTON_MODO))
                { vTaskDelay(50 / portTICK_PERIOD_MS);}
                xEventGroupSetBits(Evento->Eventos_task,Cambio_cronometro_reloj);
            }
        }
            vTaskDelay(100 / portTICK_PERIOD_MS);
    }    
}

void Testeo_Botones_Reloj(void *parameters){ //Determina los estados del funcionamiento
    Reloj_t Evento = (Reloj_t) parameters;
    static uint8_t Estado_alarma=0;
    EventBits_t Bits_evento;
    while(1){
        Bits_evento=xEventGroupWaitBits(Evento->Eventos_reloj,
                            Botones_comunes,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        if (!gpio_get_level(BOTON_Alarma)) //Setear/apagar alarma. Activada o no
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_Alarma)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            if ((Bits_evento&Alarma_sonando)!=0){
                xEventGroupClearBits(Evento->Eventos_reloj, Alarma_sonando);
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawFilledRectangle(0,0,10,240,ILI9341_BLACK);
                ILI9341DrawFilledRectangle(0,240,320,230,ILI9341_BLACK);
                ILI9341DrawFilledRectangle(320,240,310,0,ILI9341_BLACK);
                ILI9341DrawFilledRectangle(320,0,0,10,ILI9341_BLACK);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
            }
            else{
                Estado_alarma^=1;
                if(Estado_alarma!=0){
                    xEventGroupSetBits(Evento->Eventos_reloj,Alarma_estado);
                    xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                    ILI9341DrawString(48,140,"ALARMA SETEADA",&font_16x26,ILI9341_BLUE,DIGITO_APAGADO);
                    xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                }
                else{
                    xEventGroupClearBits(Evento->Eventos_reloj,Alarma_estado);
                    xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                    ILI9341DrawString(48,140,"ALARMA SETEADA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
                    xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                }
            }
        }
        else if (!gpio_get_level(BOTON_TC0)) //Configurar alarma, horario
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC0)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupSetBits(Evento->Eventos_reloj,Seteando_alarma); 
            xEventGroupClearBits(Evento->Eventos_reloj,Botones_comunes);
            xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
            ILI9341DrawString(48,140,"ALARMA SETEADA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
            xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
        }
        else if (!gpio_get_level(BOTON_TC1)) //Configurar el horario del reloj.
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            while (!gpio_get_level(BOTON_TC1)){
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            xEventGroupClearBits(Evento->Eventos_reloj,(Botones_comunes | Reloj_estado));
            xEventGroupSetBits(Evento->Eventos_reloj,Seteando_hora); 
        }
        else if (!gpio_get_level(BOTON_MODO)){
            vTaskDelay(200 / portTICK_PERIOD_MS);
            if (!gpio_get_level(BOTON_MODO)){
                while(!gpio_get_level(BOTON_MODO))
                { vTaskDelay(50 / portTICK_PERIOD_MS);}
                xEventGroupSetBits(Evento->Eventos_reloj,Cambio_reloj_cronometro);
            }
        }
            vTaskDelay(100 / portTICK_PERIOD_MS);
    }    
}

/*ILI9341DrawString(24,140,"CONFIGURANDO HORA",&font_16x26,ILI9341_BLUE,DIGITO_APAGADO);
            ILI9341DrawString(48,190,"ALARMA SETEADA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);*/
void Cambiar_parametros_reloj (void* parameters){
    Reloj_t Evento = (Reloj_t) parameters;
    EventBits_t Bits_Eventos;
    static uint32_t Dato_recibir=0;
    static uint32_t Dato_enviar=0;
    static uint32_t Conteo_local=0;
    static uint32_t Conteo_local_h=0;
    static uint32_t Alarma_previa=0;
    static uint32_t Alarma_previa_h=0;
    static uint8_t Mostrar_en_pantalla=1;
    static uint32_t *Variable_interes;
    static uint32_t *Variable_interes_h;

    while(1){
        Bits_Eventos=xEventGroupWaitBits(Evento->Eventos_reloj,
                    (Seteando_alarma | Seteando_hora),
                    pdFALSE,
                    pdFALSE,
                    portMAX_DELAY);
        xQueueReceive(Evento->Variable_Reloj_Actual,(void*)&Dato_recibir,0);
        if (Mostrar_en_pantalla){
            if((Bits_Eventos & Seteando_alarma)!=0){
                Variable_interes=&Alarma_previa;
                Variable_interes_h=&Alarma_previa_h;
            }
            else if ((Bits_Eventos & Seteando_hora)!=0){
                Conteo_local=Dato_recibir%60;
                Conteo_local_h=(Dato_recibir/60)%24;
                Variable_interes=&Conteo_local;
                Variable_interes_h=&Conteo_local_h;
            }
                xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                ILI9341DrawFilledCircle(160, 20, 5, DIGITO_ENCENDIDO);
                ILI9341DrawFilledCircle(160, 100, 5, DIGITO_ENCENDIDO);
                xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                Dato_enviar=(*Variable_interes_h)*60+*Variable_interes;
                xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_hora_r | Pantalla_min_r);
                xQueueSend(Evento->Variable_Reloj,(void*)Variable_interes, portMAX_DELAY);
                xQueueSend(Evento->Variable_Reloj_hora,(void*)&Dato_enviar, portMAX_DELAY);
                Mostrar_en_pantalla=0;
        }

        if (!gpio_get_level(BOTON_TC0)){ //Aumentar minutos
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (*(Variable_interes)==59){
                *Variable_interes=0;
            }
            else{
                *Variable_interes+=1;
            }
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_min_r);
            xQueueSend(Evento->Variable_Reloj,(void*)Variable_interes,portMAX_DELAY);
        }
        else if (!gpio_get_level(BOTON_TC1)){ //Disminuir minutos
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (*(Variable_interes)==0){
                *Variable_interes=59;
            }
            else{
                *Variable_interes-=1;
            }
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_min_r);
            xQueueSend(Evento->Variable_Reloj,(void*)Variable_interes,portMAX_DELAY);
        }
        else if (!gpio_get_level(BOTON_TC2)){ //Aumentar hora
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (*(Variable_interes_h)==23){
                *Variable_interes_h=0;
            }
            else{
                *Variable_interes_h+=1;
            }
            Dato_enviar=*Variable_interes_h*60;
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_hora_r);
            xQueueSend(Evento->Variable_Reloj_hora,(void*)&(Dato_enviar),portMAX_DELAY);
        }
        else if (!gpio_get_level(BOTON_Alarma)){ //Disminuir hora
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (*(Variable_interes_h)==0){
                *Variable_interes_h=23;
            }
            else{
                *Variable_interes_h-=1;
            }
            Dato_enviar=(*Variable_interes_h)*60;
            xEventGroupSetBits(Evento->Eventos_reloj,Pantalla_hora_r);
            xQueueSend(Evento->Variable_Reloj_hora,(void*)&(Dato_enviar),portMAX_DELAY);
        }
        else if (!gpio_get_level(BOTON_MODO)){ //Finalizar con los cambios.
            vTaskDelay(200/portTICK_PERIOD_MS);
            if (!gpio_get_level(BOTON_MODO)){
                while(!gpio_get_level(BOTON_MODO)){
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                Dato_enviar=*Variable_interes+(*Variable_interes_h)*60;
                if ((Bits_Eventos & Seteando_alarma)!=0){
                    xEventGroupClearBits(Evento->Eventos_reloj, Seteando_alarma);
                    xEventGroupSetBits(Evento->Eventos_reloj, (Botones_comunes | Actualizar_pantalla));
                    xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                    ILI9341DrawString(8,140,"CONFIGURANDO ALARMA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
                    xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                    xQueueSend(Evento->Variable_Reloj_Alarma,(void*)&Dato_enviar,portMAX_DELAY);
                }
                else if ((Bits_Eventos & Seteando_hora)!=0){
                    xEventGroupClearBits(Evento->Eventos_reloj, Seteando_hora);
                    xQueueSend(Evento->Variable_Reloj_Actual,(void*)&Dato_enviar,portMAX_DELAY);
                    xQueueSend(Evento->Variable_Reloj_nueva,(void*)&Dato_enviar,portMAX_DELAY);
                    xEventGroupSetBits(Evento->Eventos_reloj, (Reloj_estado | Botones_comunes | Actualizar_pantalla | Pantalla_modo_hora)); //Eventos_control
                    xSemaphoreTake(Evento->Semaforo_SPI_Reloj,portMAX_DELAY);
                    ILI9341DrawString(24,140,"CONFIGURANDO HORA",&font_16x26,DIGITO_APAGADO,DIGITO_APAGADO);
                    xSemaphoreGive(Evento->Semaforo_SPI_Reloj);
                }
                Mostrar_en_pantalla=1;
                }
            }
        else{
            vTaskDelay(100/ portTICK_PERIOD_MS);
        }
        
    } 
}  

void Cambiar_contexto_reloj_a_cronometro (void * parameters){
    Cronometro_t Eventos = (Cronometro_t) parameters;
    static uint32_t Cronometro_backup=0;
    static uint32_t Segundos=0;
    static uint32_t Decimas=0;
    static uint32_t Minutos=0;
    while(1){
        xEventGroupWaitBits(Eventos->Eventos_reloj,
            Cambio_reloj_cronometro, //Eventos_control
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);
            xEventGroupClearBits(Eventos->Eventos_reloj, Pantalla_modo_hora | Botones_comunes);
            xSemaphoreTake(Eventos->Semaforo_SPI,portMAX_DELAY);
            ILI9341Fill(ILI9341_BLACK);
            ILI9341DrawFilledCircle(130, 80, 5, DIGITO_ENCENDIDO);
            ILI9341DrawFilledCircle(230, 80, 5, DIGITO_ENCENDIDO);
            xSemaphoreGive(Eventos->Semaforo_SPI);
            //Bits_eventos=xEventGroupGetBits(Eventos->Eventos_task);
            xQueueReceive(Eventos->Variable_conteo_Backup,(void*)&Cronometro_backup,100 / portTICK_PERIOD_MS);
            Segundos=(Cronometro_backup/10)%60;
            Decimas=Cronometro_backup%10;
            Minutos=Cronometro_backup/600;
            xQueueSend(Eventos->Queue_dec,(void*)&Decimas,portMAX_DELAY);
            xQueueSend(Eventos->Queue_min,(void*)&Minutos,portMAX_DELAY);
            xQueueSend(Eventos->Queue_seg,(void*)&Segundos,portMAX_DELAY);
            xEventGroupSetBits(Eventos->Eventos_task, Refrescar_lap | Todo_seg_cro | Todo_min_cro | Pantalla_dec | Pantalla_modo_cronometro);
    }
}

void Cambiar_contexto_cronometro_a_reloj (void * parameters){
    Reloj_t Eventos = (Reloj_t) parameters;
    while(1){
        xEventGroupWaitBits(Eventos->Eventos_task,
                    Cambio_cronometro_reloj, //Eventos_control
                    pdTRUE,
                    pdFALSE,
                    portMAX_DELAY);
        xEventGroupClearBits(Eventos->Eventos_task, Pantalla_modo_cronometro);
        xSemaphoreTake(Eventos->Semaforo_SPI_Reloj,portMAX_DELAY);
        ILI9341Fill(ILI9341_BLACK);
        xSemaphoreGive(Eventos->Semaforo_SPI_Reloj);
        xEventGroupSetBits(Eventos->Eventos_reloj, Actualizar_pantalla | Botones_comunes);
                }
            }