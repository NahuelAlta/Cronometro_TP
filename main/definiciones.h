#ifndef _DEFINICIONES_H_
#define _DEFINICIONES_H_

#include "digitos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
//-------------------------------------------------------
//----------- Definicion de bits asociados a eventos de cronómetro --//
#define Cronometro_estado   1 //Eventos_control
#define Interrupcion_TC0    (1 << 1)
#define Interrupcion_TC1    (1 << 2)
#define Interrupcion_TC2    (1 << 3)
#define Resetear_Ev         (1 << 4)
#define Todo_min_cro        (1 << 5)
#define Pantalla_dec        (1 << 6)
#define Pantalla_seg        (1 << 7)
#define Pantalla_min        (1 << 8)
#define Refrescar_lap               (1 << 10)
#define Todo_seg_cro                (1 << 11)
#define Pantalla_modo_cronometro    (1 << 12)
#define Cambio_cronometro_reloj     (1 << 13)
#define Resetear_laps               (1 << 14)
//--------------------------------------------------------------
//----------- Definicion de bits asociados a eventos de reloj --//
#define Pantalla_modo_hora  1 //Eventos_control
#define Alarma_estado       (1 << 1)
#define Actualizar_pantalla (1 << 2)
#define Reloj_estado        (1 << 3) //lleva la cuenta y muestra en pantalla. 1 Funciona, 0 No funciona
#define Seteando_hora       (1 << 4) //No lleva la cuenta ni muestra en pantalla. Reloj estado debe estar en 0
#define Seteando_alarma     (1 << 5) //Lleva la cuenta y no muestra en pantalla. Reloj estado debe estar en 1 
#define Botones_comunes     (1 << 6)
#define Pantalla_min_r      (1 << 7)
#define Pantalla_hora_r     (1 << 8)
#define Todo_min_r          (1 << 9)
#define Todo_hora_r                 (1 << 10)
#define Digitos_seteados_M          (1 << 11)
#define Digitos_seteados_H          (1 << 12)
#define Cambio_reloj_cronometro     (1 << 13)
#define Alarma_sonando              (1 << 14)
//---------- Definicion de estados --------------------//
#define CORRIENDO       1
#define PARADO          0
//-------------------------------------------------------
//---------- Definicion de Pin de botones -------------//
#define BOTON_TC0       2
#define BOTON_TC1       8
#define BOTON_TC2       9
#define BOTON_MODO      21
#define BOTON_Alarma    1
//-------------------------------------------------------
//---------- Definiciones para paneles de Display -----//
#define DIGITO_ANCHO     40
#define DIGITO_ALTO      80
#define LAP_ANCHO        25
#define LAP_ALTO         50
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO   0x3800
#define DIGITO_FONDO     ILI9341_BLACK

typedef struct Reloj_s {
    QueueHandle_t Variable_Reloj;
    QueueHandle_t Variable_Reloj_hora;
    QueueHandle_t Variable_Reloj_nueva;
    QueueHandle_t Variable_Reloj_Alarma;
    QueueHandle_t Variable_Reloj_Backup;
    QueueHandle_t Variable_Reloj_Actual;
    EventGroupHandle_t Eventos_reloj;
    EventGroupHandle_t Eventos_task;
    SemaphoreHandle_t Semaforo_SPI_Reloj;
    panel_t Panel_hora_reloj;
    panel_t Panel_min_reloj;
}*Reloj_t;

typedef struct Cronometro_s {
    SemaphoreHandle_t Semaforo_SPI;
    panel_t Panel_dec;
    panel_t Panel_seg;
    panel_t Panel_min;
    EventGroupHandle_t Eventos_task;
    EventGroupHandle_t Eventos_reloj;
    QueueHandle_t Variable_conteo;
    QueueHandle_t Queue_dec;
    QueueHandle_t Queue_seg;
    QueueHandle_t Queue_min;
    QueueHandle_t Cola_lap;
    QueueHandle_t Variable_conteo_Backup;
}* Cronometro_t;

//-----------------------------------------------------------------------------------------
//--------------- Declaracion de estructura asociada a cada seccion de panel de LAP ----//
typedef struct Panel_Lap_s {
    panel_t Minutos_panel;
    panel_t Segundos_panel;
    panel_t Decimas_panel;
} Lap_panel;
//--------------------------------------------------------------------------------------
//--------------- Declaracion de estructura usada en obtención de LAP ----------------//
typedef struct Pantalla_Lap_s {
    Lap_panel     Paneles[2];
    QueueHandle_t       Cola_lap;
    EventGroupHandle_t  Eventos_task;
    SemaphoreHandle_t   Semaforo_SPI;
} *Pantalla_Lap_t;

#endif