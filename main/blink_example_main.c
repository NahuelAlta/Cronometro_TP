#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "digitos.h"
#include "fonts.h"
#include "ili9341.h"

//--------Definiciones de pin para leds ---------------//
#define LED_ROJO        0   
#define LED_VERDE       1
//-------------------------------------------------------
//---------- Definicion de estados --------------------//
#define CORRIENDO       1
#define PARADO          0
//-------------------------------------------------------
//---------- Definicion de Pin de botones -------------//
#define BOTON_TC0       2
#define BOTON_TC1       8
#define BOTON_TC2       9
//-------------------------------------------------------
//----------- Definicion de bits asociados a eventos --//
#define Interrupcion_TC0    (1 << 1)
#define Interrupcion_TC1    (1 << 2)
#define Interrupcion_TC2    (1 << 3)
#define Cambio_min          (1 << 4)
#define Cambio_seg          (1 << 5)
#define Cambio_dec          (1 << 6)
#define Resetear_Ev         (1 << 7)
#define Pantalla_Lap        (1 << 8)
#define Pantalla_dec        (1 << 9)
#define Pantalla_seg        (1 << 10)
#define Pantalla_min        (1 << 11)
//-------------------------------------------------------
//---------- Definiciones para paneles de Display -----//
#define DIGITO_ANCHO     40
#define DIGITO_ALTO      80
#define LAP_ANCHO        25
#define LAP_ALTO         50
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO   0x3800
#define DIGITO_FONDO     ILI9341_BLACK
//-------------------------------------------------------
//---------- Declaraciones variables globales --------//
uint32_t Conteo         = 0;
uint8_t Estado_Global   = 0;
//-----------------------------------------------------------------------
//---------------- Creación de estructura para tareas de control ------//
typedef struct Cronometro_s {
    SemaphoreHandle_t Semáforo_variable; 
    SemaphoreHandle_t Semaforo_Estado_G;
    SemaphoreHandle_t Semaforo_SPI;
    panel_t Panel_dec;
    panel_t Panel_seg;
    panel_t Panel_min;
    EventGroupHandle_t Eventos_task;
    QueueHandle_t Variable_conteo;
    QueueHandle_t Queue_dec;
    QueueHandle_t Queue_seg;
    QueueHandle_t Queue_min;
    QueueHandle_t Cola_lap;
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
    Lap_panel     panel_1;
    Lap_panel     panel_2;
    QueueHandle_t       Cola_lap;
    EventGroupHandle_t  Eventos_task;
    SemaphoreHandle_t   Semaforo_SPI;
} *Pantalla_Lap_t;
//----------------------------------------------------------
//------------------- Task FreeRTOS ----------------------//
// Testeo Botones:
/*      Esta task lo que hace es ir controlando el estado de cada pin asociado a los botones. 
        Si se cumple que uno fue puesto a masa, espera un delay de 50ms y queda esperando que 
        dicho boton vuelva a estar en alto, con la idea de evitar toques accidentales y funcionar
        como una tarea de antirrebote. Una vez que se cumplio la condicion, setea en uno el bit
        correspondiente al evento pertinente segun el boton apretado
----------------------------------------------------------*/
static void Testeo_Botones(void *args){
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
/*  ----------------------------------------------------------
    Parpadeo_led_verde:
        Lo que hace esta tarea es parpadear el led con un período de 250ms, de manera que el 
        led se encuentra encendido cada 500ms. Debido a que es una tarea de baja prioridad, no
        se implementó el "vTaskDelayUntil" y se implementó simplemente vTaskDelay. Además, si 
        el estado global es 0, no lleva a cabo ninguna tarea. Debido a que no es una tarea 
        de alta prioridad, no se implementó el semaforo sobre la variable global de "Estado_Global"
-------------------------------------------------------------*/
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
/*  ----------------------------------------------------------
    Start_Stop_Cronometro:
        Esta tarea espera que el bit 1 de la palabra asociada a eventos se coloque en 1. 
        Una vez que se cumple dicha condición pide, por medio de un semáforo, el acceso a la variable
        global de "Estado_Global" y modifica su valor. El valor al cual se modifica la variable global
        depende de si se encuentra "Parado" o "Corriendo". En cualquiera de los casos, le asigna a dicha 
        variable, el estado opuesto. Una vez realizado, devuelve el semáforo y prende o apaga el led Rojo 
        según corresponda. 
-------------------------------------------------------------*/
static void Start_Stop_Cronometro (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
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
/*  ----------------------------------------------------------
    Reset_Cronometro:
        Esta tarea espera que el bit 2 de la palabra asociada a eventos se coloque en 1. 
        Una vez que se cumple dicha condición pide, por medio de un semáforo, el acceso a la variable
        global de "Estado_Global" y evalua su valor para conocer el estado del cronómetro. Si el estado
        es "Parado" entonces, a través de un semáforo, pide acceso a la variable global "Conteo" y la 
        setea en cero, reiniciando el cronómetro. Una vez realizado, setea los bits, en la palabra asociada
        a eventos, correspondientes al cambio en segundos, decimas y minutos, los cuales sirven de 
        notificacion para conocer cuando actualizar la pantalla y que parte del panel (Tareas de Actualizar_valores,
        Actualizar_pantalla_dec, Actualizar_pantalla_seg y Actualizar_pantalla_min).  
        Si el estado de la variable global es corriendo, solamente devuelve el semáfaro. 
-------------------------------------------------------------*/
static void Reset_Cronometro (void *parameters){
    Cronometro_t Estructura = (Cronometro_t) parameters;
    EventBits_t Bits_return = 0;
    static uint32_t Nulo_lap = 0; 
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
                xQueueSend(Estructura->Variable_conteo,(void*)&Conteo,portMAX_DELAY);
                xEventGroupSetBits(Estructura->Eventos_task,(Cambio_dec|Cambio_min|Cambio_seg));
                xQueueSend(Estructura->Cola_lap,(void*)&Nulo_lap,portMAX_DELAY);
                xQueueSend(Estructura->Cola_lap,(void*)&Nulo_lap,portMAX_DELAY);
            }
            else{
                xSemaphoreGive(Estructura->Semaforo_Estado_G);
            }
        }
    }
}
/*  ----------------------------------------------------------
    Obtener_Lap:
        Esta tarea espera al evento asociado a la interrupción por el boton TC2. Una vez 
        que se produjo dicho evento, accede a la variable globlal de "Conteo" a través de un 
        semáforo y guarda su estado en una variable local. Posteriormente, utiliza esa variable
        local para enviarla a la tarea correspondiente a mostrar en pantalla el tiempo obtenido
        (Mostrar_LAP).
-------------------------------------------------------------*/
static void Obtener_Lap (void *parameters){
    Cronometro_t    Estructura      = (Cronometro_t) parameters;
    //uint32_t        Lap_contador    = 0;
    while(1){
        xEventGroupWaitBits(Estructura->Eventos_task,
                            Interrupcion_TC2,
                            pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);
        // xSemaphoreTake(Estructura->Semáforo_variable,portMAX_DELAY);
        // Lap_contador=Conteo;
        // xSemaphoreGive(Estructura->Semáforo_variable);
        xQueueSend(Estructura->Cola_lap,(void *)&Conteo,portMAX_DELAY);
    }
}
/*  ----------------------------------------------------------
    Mostrar_LAP:
        Esta tarea espera a que haya un dato en la cola. Una vez que se obtiene dicho dato,
        (el cual es igual al contador obtenido en Obtener_LAP) lo utiliza para calcular los
        minutos, segundos y decimas que llevaba el cronómetro al obtener el LAP. Luego, plasma los 
        resultados en el panel asociado a la obtencion de LAPs. 
        El panel de los laps posee la capacidad para almacenar dos tiempos, la tarea evalua a través
        de la variable panel_n cual fue el panel que se utilizó ultimo y alternalo en al proxima
        iteración.
-------------------------------------------------------------*/
static void Mostrar_Lap (void *parameters){
    Pantalla_Lap_t Estructura = (Pantalla_Lap_t) parameters;
    static uint32_t minutos=0;
    static uint32_t segundos=0;
    static uint32_t decimas=0;
    static uint32_t panel_n = 0;
    static uint32_t Buffer_dato = 0;
    static Lap_panel Panel;
    while(1){
        xQueueReceive(Estructura->Cola_lap, &Buffer_dato, portMAX_DELAY);
        decimas = Buffer_dato%10;
        segundos = (Buffer_dato/10)%60;
        minutos = Buffer_dato/600;
        if (!panel_n){
            Panel = Estructura->panel_1;
            xSemaphoreTake(Estructura->Semaforo_SPI,portMAX_DELAY);
            ILI9341DrawFilledCircle(135,156,2,ILI9341_RED);
            ILI9341DrawFilledCircle(195,156,2,ILI9341_RED);
        }
        else{
            Panel = Estructura->panel_2;
            xSemaphoreTake(Estructura->Semaforo_SPI,portMAX_DELAY);
            ILI9341DrawFilledCircle(135,226,2,ILI9341_RED);
            ILI9341DrawFilledCircle(195,226,2,ILI9341_RED);
        }
        DibujarDigito(Panel.Decimas_panel,0,decimas);
        DibujarDigito(Panel.Decimas_panel,0,decimas);
        DibujarDigito(Panel.Segundos_panel,0,segundos/10);
        DibujarDigito(Panel.Segundos_panel,1,segundos%10);
        DibujarDigito(Panel.Minutos_panel,0,minutos/10);
        DibujarDigito(Panel.Minutos_panel,1,minutos%10);
        panel_n^=1;
        xSemaphoreGive(Estructura->Semaforo_SPI);   
    }
}
/*  ----------------------------------------------------------
    Contador:
        Esta tarea es la de mayor prioridad. Primero, evalua el estado del cronometro, el cual
        puede ser "Corriendo" o "parado", para ello, accede al mismo a traves de un semaforo. 
        Una vez que tiene acceso de dicho semaforo, se evalua su estado, y se lo devuelve en la linea 
        siguiente. Si el cronometro esta corriendo, la tarea realiza un Delay exacto de 100ms, pasado ese tiempo,
        Accede a la variable global de "Conteo" a través de un semaforo, la incrementa, asigna su valor a una 
        variable local y devuelve el semáforo. 
        Posteriormente, utiliza la variable local de conteo para indicar si hubo un cambio en las decimas, segundos
        y/o en los minutos, seteando los bits de eventos correspondientes. 
-------------------------------------------------------------*/
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
            xQueueSend(parametros->Variable_conteo,(void*)&Conteo,portMAX_DELAY);
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
/*  ----------------------------------------------------------
    Actualizar_valores:
        Esta tarea tiene la función de determinar cuales fueron las unidades de tiempo que sufrieron
        cambios, para asi actualizar solamente esa sección del panel y evitar que parpadeen el resto de números.
        Dependiendo cual sea el cambio correspondiente, envia en una cola el dato de decimas, segundos o minutos
        obtenido a partir de la variable global "Conteo".  
-------------------------------------------------------------*/
static void Actualizar_valores(void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint32_t Conteo_local = 0;
    EventBits_t Bits_Evento = 0;
    static uint32_t Decimas=0;
    static uint32_t Segundos=0;
    static uint32_t Minutos=0;
    while(1){
        Bits_Evento = xEventGroupWaitBits(
            parametros->Eventos_task,    
            Cambio_dec | Cambio_seg | Cambio_min,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
        // xSemaphoreTake(parametros->Semáforo_variable,portMAX_DELAY);
        // Conteo_local = Conteo;
        // xSemaphoreGive(parametros->Semáforo_variable);
        xQueueReceive(parametros->Variable_conteo,(void*)&Conteo_local,pdMS_TO_TICKS(50));
        if ((Bits_Evento & Cambio_dec) != 0){
            Decimas = Conteo_local%10;
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_dec);
            xQueueSend(parametros->Queue_dec,(void *)&Decimas,portMAX_DELAY);
        }
        if ((Bits_Evento & Cambio_seg) != 0){
            Segundos = (Conteo_local/10)%60;
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_seg);
            xQueueSend(parametros->Queue_seg,(void *)&Segundos,portMAX_DELAY);
        }
        if ((Bits_Evento & Cambio_min) != 0){
            Minutos = Conteo_local/600;
            xEventGroupSetBits(parametros->Eventos_task,Pantalla_min);
            xQueueSend(parametros->Queue_min,(void *)&Minutos,portMAX_DELAY);
        }
    }
}
/*  ----------------------------------------------------------
    Actualizar_pantalla_dec/seg/min:
        Estas tareas esperan a dos sucesos, uno, el evento que indique que es necesario actualizar la pantalla.
        Dichos eventos son los asociados a los bits de cambio, "Cambio_dec, Cambio_seg, Cambio_min". 
        Una vez que se obtuvieron estos eventos, se espera a que haya un dato en la cola correspondiente. 
        Recibido dicho dato, lleva a cabo la gráfica en el panel que corresponda.
-------------------------------------------------------------*/
void Actualizar_pantalla_dec (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Decena = 0;
    static uint32_t Decimas_local=0;
    while(1){
            xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_dec,  
            pdTRUE,         
            pdFALSE,
            portMAX_DELAY);
            xQueueReceive(parametros->Queue_dec,(void *)&Decimas_local,portMAX_DELAY);
            Decena = Decimas_local%10;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            DibujarDigito(parametros->Panel_dec,0,Decena);
            xSemaphoreGive(parametros->Semaforo_SPI);

    }
}

void Actualizar_pantalla_seg (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    static uint32_t Segundos_local = 0;
    while(1){
        xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_seg,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
            xQueueReceive(parametros->Queue_seg,(void *)&Segundos_local,portMAX_DELAY);
            Unidad = Segundos_local%10;
            Decena = Segundos_local/10;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            DibujarDigito(parametros->Panel_seg,0,Decena);
            DibujarDigito(parametros->Panel_seg,1,Unidad);
            xSemaphoreGive(parametros->Semaforo_SPI);
        
    }
}

void Actualizar_pantalla_min (void *parameters){
    Cronometro_t parametros = (Cronometro_t) parameters;
    static uint8_t Unidad = 0;
    static uint8_t Decena = 0;
    static uint32_t Minutos_local = 0;
    while(1){
        xEventGroupWaitBits(
            parametros->Eventos_task,    
            Pantalla_min,  
            pdTRUE,         
            pdFALSE,        
            portMAX_DELAY);
            xQueueReceive(parametros->Queue_min,(void *)&Minutos_local,portMAX_DELAY);
            Decena = Minutos_local/10;
            Unidad = Minutos_local%10;
            xSemaphoreTake(parametros->Semaforo_SPI,portMAX_DELAY);
            DibujarDigito(parametros->Panel_min,0,Decena);
            DibujarDigito(parametros->Panel_min,1,Unidad);
            xSemaphoreGive(parametros->Semaforo_SPI);
        
    }
}
//----------------------------------------------------------

void app_main (void){

    //----------- Declaracion de semaforos y colas -------//
    QueueHandle_t  Queue_Decimas; 
    QueueHandle_t  Queue_Segundos; 
    QueueHandle_t  Queue_Minutos; 
    QueueHandle_t  Queue_Conteo;
    SemaphoreHandle_t  Semaforo_Estado_Cronometro;
    SemaphoreHandle_t  Semaforo_Conteo_Global;
    SemaphoreHandle_t  Periférico_SPI;
    //-------------------------------------------------------
    //----------- Asignación de semáforos y colas ---------//  
    Queue_Decimas                =xQueueCreate(1,sizeof(uint32_t));
    Queue_Segundos               =xQueueCreate(1,sizeof(uint32_t));
    Queue_Minutos                =xQueueCreate(1,sizeof(uint32_t));
    Queue_Conteo                 =xQueueCreate(1,sizeof(uint32_t));
    Semaforo_Estado_Cronometro   =xSemaphoreCreateMutex();
    Semaforo_Conteo_Global       =xSemaphoreCreateMutex();
    Periférico_SPI               =xSemaphoreCreateMutex();
    //---------------------------------------------------------------
    //----------- Asignación a estructura para argumentos de Task -//
    //----------------asociadas al control del cronómetro ---------//  
    static struct Cronometro_s Control_temporal;

    EventGroupHandle_t Grupo_eventos;
    Grupo_eventos = xEventGroupCreate();
    
    QueueHandle_t Cronometro_lap;
    Cronometro_lap = xQueueCreate(2,sizeof(uint32_t));
    
    Control_temporal.Eventos_task=      Grupo_eventos;
    Control_temporal.Semaforo_Estado_G=     Semaforo_Estado_Cronometro; // Si está corriendo o esta parado
    Control_temporal.Semaforo_SPI=          Periférico_SPI;
    Control_temporal.Semáforo_variable=     Semaforo_Conteo_Global; 
    Control_temporal.Queue_dec=         Queue_Decimas;
    Control_temporal.Queue_min=         Queue_Minutos;
    Control_temporal.Queue_seg=         Queue_Segundos;
    Control_temporal.Variable_conteo=   Queue_Conteo;
    Control_temporal.Cola_lap=          Cronometro_lap;
    //-------------------------------------------------
    //----------- Inicializacion de display ---------// 
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);
    //---------------------------------------------------------
    //----------- Creación de paneles de cronómetro ---------//  
    Control_temporal.Panel_dec=CrearPanel(240, 10, 1, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_seg=CrearPanel(140, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Control_temporal.Panel_min=CrearPanel(40, 10, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //------------------------------------------------------------
    //-------- Inicializacion de numeros de crónometro ---------//
    //--------------------  Formato MM.SS.D --------------------//    
    DibujarDigito(Control_temporal.Panel_dec, 0, 0);
    DibujarDigito(Control_temporal.Panel_seg, 0, 0);
    DibujarDigito(Control_temporal.Panel_seg, 1, 0);
    DibujarDigito(Control_temporal.Panel_min, 0, 0);
    DibujarDigito(Control_temporal.Panel_min, 1, 0);
    ILI9341DrawFilledCircle(130, 80, 5, DIGITO_ENCENDIDO);
    ILI9341DrawFilledCircle(230, 80, 5, DIGITO_ENCENDIDO);
    //-------------------------------------------------------------------------
    //----------- Creación de estructura para task asociadas al LAP ---------//  
    static struct Pantalla_Lap_s Estructura_vueltas;
    Estructura_vueltas.Cola_lap=Cronometro_lap;
    Estructura_vueltas.Semaforo_SPI=Periférico_SPI;
    Estructura_vueltas.Eventos_task=Grupo_eventos;
    //--------------------------------------------------------
    //----------- Creacion paneles de LAP superior ---------//    
    Estructura_vueltas.panel_1.Decimas_panel=CrearPanel(200, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.panel_1.Segundos_panel=CrearPanel(140, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.panel_1.Minutos_panel=CrearPanel(80, 110, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //--------------------------------------------------------
    //----------- Creacion paneles de LAP Inferior ---------// 
    Estructura_vueltas.panel_2.Decimas_panel=CrearPanel(200, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.panel_2.Segundos_panel=CrearPanel(140, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    Estructura_vueltas.panel_2.Minutos_panel=CrearPanel(80, 180, 2, LAP_ALTO, LAP_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    //-------------------------------------------------
    //----------- Configuracion pines boton ---------//
    gpio_config_t io_conf_int = {};
    io_conf_int.pin_bit_mask =
        ((1ULL << BOTON_TC0) | (1ULL << BOTON_TC1) | (1ULL << BOTON_TC2));
    io_conf_int.mode = GPIO_MODE_INPUT;
    io_conf_int.pull_up_en = true;
    io_conf_int.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int);
    //------------------------------------------------
    //----------- Configuracion pines LEDS ---------//
    gpio_config_t io_conf_int2 = {};
    io_conf_int2.pin_bit_mask =
        ((1ULL << LED_ROJO) | (1ULL << LED_VERDE));
    io_conf_int2.mode = GPIO_MODE_OUTPUT;
    io_conf_int2.pull_up_en = false;
    io_conf_int2.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf_int2);
    //---------------------------------------
    //----------- Tarea principal ---------//
    xTaskCreate(Contador,"Counter_dec",6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+4,NULL);
    //---------------------------------------
    //-------------- Control --------------//
    xTaskCreate(Testeo_Botones,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    xTaskCreate(Start_Stop_Cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    xTaskCreate(Reset_Cronometro,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    xTaskCreate(Obtener_Lap,"Obtencion_Lap",4*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+3,NULL);
    //---------------------------------------
    //-------------- Pantalla --------------//
    xTaskCreate(Actualizar_valores,NULL,6*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Actualizar_pantalla_dec,"Actualizar_pantalla_dec",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Actualizar_pantalla_seg,"Actualizar_pantalla_seg",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Actualizar_pantalla_min,"Actualizar_pantalla_min",16*configMINIMAL_STACK_SIZE,(void*)&Control_temporal,tskIDLE_PRIORITY+2,NULL);
    xTaskCreate(Mostrar_Lap,"Lap_pantalla",16*configMINIMAL_STACK_SIZE,(void*)&Estructura_vueltas,tskIDLE_PRIORITY+2,NULL);
    //---------------------------------------
    //------------ Parpadeo Led -----------//
    xTaskCreate(Parpadeo_led_verde,NULL,6*configMINIMAL_STACK_SIZE,NULL,tskIDLE_PRIORITY+1,NULL);
    //---------------------------------------
}