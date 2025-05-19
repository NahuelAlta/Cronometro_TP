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
#include "cronometro.h"
#include "controlgen.h"
#include "pantalla_cronometro.h"
#include "pantalla_reloj.h"

#define MODO_RELOJ
//#define MODO_CRONOMETRO

void app_main (void){

    //----------- Declaracion de semaforos y colas -------//
    QueueHandle_t  Queue_Decimas; 
    QueueHandle_t  Queue_Segundos; 
    QueueHandle_t  Queue_Minutos; 
    QueueHandle_t  Queue_Conteo;
    QueueHandle_t  Variable_conteo_Backup_m;

    SemaphoreHandle_t  Periférico_SPI;
    //-------------------------------------------------------
    //----------- Asignación de semáforos y colas ---------//  
    Queue_Decimas                =  xQueueCreate(1,sizeof(uint32_t));
    Queue_Segundos               =  xQueueCreate(1,sizeof(uint32_t));
    Queue_Minutos                =  xQueueCreate(1,sizeof(uint32_t));
    Queue_Conteo                 =  xQueueCreate(1,sizeof(uint32_t));
    Periférico_SPI               =  xSemaphoreCreateMutex();
    Variable_conteo_Backup_m     =  xQueueCreate(1,sizeof(uint32_t));
    //---------------------------------------------------------------
    //----------- Asignación a estructura para argumentos de Task -//
    //----------------asociadas al control del cronómetro ---------//  
    static struct Cronometro_s Control_temporal;

    EventGroupHandle_t Grupo_eventos;
    Grupo_eventos = xEventGroupCreate();

    EventGroupHandle_t Grupo_eventos_h;
    Grupo_eventos_h = xEventGroupCreate();  
    
    QueueHandle_t Cronometro_lap;
    Cronometro_lap = xQueueCreate(1,sizeof(uint32_t));
    
    Control_temporal.Eventos_task       =   Grupo_eventos;
    Control_temporal.Semaforo_SPI       =   Periférico_SPI;
    Control_temporal.Queue_dec          =   Queue_Decimas;
    Control_temporal.Queue_min          =   Queue_Minutos;
    Control_temporal.Queue_seg          =   Queue_Segundos;
    Control_temporal.Variable_conteo_Backup = Variable_conteo_Backup_m;
    Control_temporal.Variable_conteo    =   Queue_Conteo;
    Control_temporal.Cola_lap           =   Cronometro_lap;
       //---------------------------------------------------------------
    //----------- Asignación a estructura para argumentos de Task -//
    //----------------asociadas al control del reloj ---------//  
    static struct Reloj_s Control_reloj;
    
    QueueHandle_t Variable_Reloj_m;
    QueueHandle_t Variable_Reloj_hora_m;
    QueueHandle_t Variable_Reloj_Alarma_m;
    QueueHandle_t Variable_Reloj_nueva_m;
    QueueHandle_t Variable_Reloj_Backup;
    QueueHandle_t Variable_Reloj_Actual;

    Variable_Reloj_m=         xQueueCreate(1,sizeof(uint32_t));
    Variable_Reloj_hora_m=    xQueueCreate(1,sizeof(uint32_t));
    Variable_Reloj_Alarma_m=  xQueueCreate(1,sizeof(uint32_t));
    Variable_Reloj_nueva_m=   xQueueCreate(1,sizeof(uint32_t));
    Variable_Reloj_Backup=    xQueueCreate(1,sizeof(uint32_t));
    Variable_Reloj_Actual=    xQueueCreate(1,sizeof(uint32_t));

    Control_reloj.Eventos_reloj       =   Grupo_eventos_h;
    Control_reloj.Semaforo_SPI_Reloj  =   Periférico_SPI;
    Control_reloj.Variable_Reloj   =     Variable_Reloj_m;
    Control_reloj.Variable_Reloj_hora   =   Variable_Reloj_hora_m;
    Control_reloj.Variable_Reloj_Alarma =   Variable_Reloj_Alarma_m;
    Control_reloj.Variable_Reloj_nueva =    Variable_Reloj_nueva_m;
    Control_reloj.Variable_Reloj_Backup=    Variable_Reloj_Backup;
    Control_reloj.Variable_Reloj_Actual=    Variable_Reloj_Actual;

    Control_temporal.Eventos_reloj= Grupo_eventos_h;
    Control_reloj.Eventos_task= Grupo_eventos;
    //-------------------------------------------------
    //----------- Inicializacion de display ---------// 
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);
    //---------------------------------------------------------
    //----------- Creación de paneles de cronómetro ---------//  
    Control_temporal.Panel_dec=CrearPanel(240, 10, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_seg=CrearPanel(140, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_min=CrearPanel(40, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //-------------------------------------------------------------------------

    Control_reloj.Panel_min_reloj = CrearPanel(170, 10, 2, 100, 60, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_reloj.Panel_hora_reloj  = CrearPanel(30, 10, 2, 100, 60, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //----------- Creación de estructura para task asociadas al LAP ---------//  
    static struct Pantalla_Lap_s Estructura_vueltas;
    Estructura_vueltas.Cola_lap         =   Cronometro_lap;
    Estructura_vueltas.Semaforo_SPI     =   Periférico_SPI;
    Estructura_vueltas.Eventos_task     =   Grupo_eventos;
    //--------------------------------------------------------
    //----------- Creacion paneles de LAP superior ---------//    
    Estructura_vueltas.Paneles[0].Decimas_panel=CrearPanel(200, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.Paneles[0].Segundos_panel=CrearPanel(140, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.Paneles[0].Minutos_panel=CrearPanel(80, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //--------------------------------------------------------
    //----------- Creacion paneles de LAP Inferior ---------// 
    Estructura_vueltas.Paneles[1].Decimas_panel=CrearPanel(200, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.Paneles[1].Segundos_panel=CrearPanel(140, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.Paneles[1].Minutos_panel=CrearPanel(80, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //----------- Configuracion pines boton ---------//
    gpio_config_t io_conf_int   = {};
    io_conf_int.pin_bit_mask    =
                ((1ULL << BOTON_TC0) | (1ULL << BOTON_TC1) | (1ULL << BOTON_TC2) | (1ULL << BOTON_MODO)
                |(1ULL << BOTON_Alarma));
    io_conf_int.mode            = GPIO_MODE_INPUT;
    io_conf_int.pull_up_en      = true;
    io_conf_int.intr_type       = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int);
    int8_t Estado_tarea=0;
    //------------------------- CRONOMETRO -----------------
    //----------- Tarea principal ---------//
    Estado_tarea=xTaskCreate(Contador,"Counter_dec",6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+4,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Contador)");
    }
    //---------------------------------------    
    //-------------- Control --------------//
    Estado_tarea=xTaskCreate(Testeo_Botones_cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+5,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Testeo_Botones_cronometro)");
    }
    Estado_tarea=xTaskCreate(Start_Stop_Cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+5,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Start_Stop_Cronometro)");
    }
    //---------------------------------------    
    //-------------- Pantalla --------------//
    Estado_tarea=xTaskCreate(Actualizar_valores,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_valores)");
    }
    Estado_tarea=xTaskCreate(Actualizar_pantalla_dec,"Actualizar_pantalla_dec",8*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_pantalla_dec)");
    }
    Estado_tarea=xTaskCreate(Actualizar_pantalla_seg,"Actualizar_pantalla_seg",8*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_pantalla_seg)");
    }
    Estado_tarea=xTaskCreate(Actualizar_pantalla_min,"Actualizar_pantalla_min",8*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_pantalla_min)");
    }
    Estado_tarea=xTaskCreate(Mostrar_Lap,"Lap_pantalla",8*configMINIMAL_STACK_SIZE,(void*)&Estructura_vueltas,tskIDLE_PRIORITY+2,NULL);
    //---------------------------------------
    //------------ Parpadeo Led -----------//
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Mostrar_Lap)");
    }
    Estado_tarea=xTaskCreate(Parpadeo_led_verde,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+1,NULL);
    //----------------------------------------------------
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Parpadeo_led_verde)");
    }
    Estado_tarea=xTaskCreate(Actualizar_pantalla_min_reloj,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_pantalla_min_reloj)");
    }
    Estado_tarea=xTaskCreate(Actualizar_pantalla_hora_reloj,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Actualizar_pantalla_hora_reloj)");
    }
    Estado_tarea=xTaskCreate(Refrescar_hora_pantalla,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+5,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Refrescar_hora_pantalla)");
    }
    Estado_tarea=xTaskCreate(Testeo_Botones_Reloj,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+5,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Testeo_Botones_Reloj)");
    }
    Estado_tarea=xTaskCreate(Reloj_cuenta,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+4,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Reloj_cuenta)");
    }
    Estado_tarea=xTaskCreate(Cambiar_parametros_reloj,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+5,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Cambiar_parametros_reloj)");
    }
    Estado_tarea=xTaskCreate(Estado_configuracion,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+2,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Estado_configuracion)");
    }
    Estado_tarea=xTaskCreate(Cambiar_contexto_cronometro_a_reloj,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+6,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Cambiar_contexto_cronometro_a_reloj)");
    }
    Estado_tarea=xTaskCreate(Cambiar_contexto_reloj_a_cronometro,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+6,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Cambiar_contexto_reloj_a_cronometro)");
    }
    Estado_tarea=xTaskCreate(Alarma_notificacion,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+3,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Cambiar_contexto_reloj_a_cronometro)");
    }
    Estado_tarea=xTaskCreate(Alarma_evento,NULL,8*configMINIMAL_STACK_SIZE,(void*)&Control_reloj,tskIDLE_PRIORITY+3,NULL);
    if(Estado_tarea==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY){
        ESP_LOGE("Creacion de tareas:","No se pudo crear la tarea anterior (Cambiar_contexto_reloj_a_cronometro)");
    }
    //--------------------------------------------------------------------
    //---------- Inicializacion de numeros de Reloj y estado -------------//

    #if defined(MODO_CRONOMETRO)
        xSemaphoreTake(Control_temporal.Semaforo_SPI,portMAX_DELAY);
        DibujarDigito(Control_temporal.Panel_dec, 0, 0);
        DibujarDigito(Control_temporal.Panel_min, 0, 0);
        DibujarDigito(Control_temporal.Panel_seg, 0, 0);
        DibujarDigito(Control_temporal.Panel_min, 1, 0);
        DibujarDigito(Control_temporal.Panel_seg, 1, 0);
        ILI9341DrawFilledCircle(130, 80, 5, DIGITO_ENCENDIDO);
        ILI9341DrawFilledCircle(230, 80, 5, DIGITO_ENCENDIDO);
        xSemaphoreGive(Control_reloj.Semaforo_SPI_Reloj);
        xEventGroupSetBits(Control_temporal.Eventos_task, Pantalla_modo_cronometro);
    #elif defined(MODO_RELOJ)
        xEventGroupSetBits(Control_reloj.Eventos_reloj, Seteando_hora);
    #endif
}