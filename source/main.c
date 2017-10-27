/**
 * This is template for main module created by New Kinetis SDK 2.x Project Wizard. Enjoy!
 **/

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_port.h"
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MAX_LOG_LENGTH 20
//#define pdMS_TO_TICS( xTimeInMs ) ( ( TickTypet ) ( ( ( uint64t ) ( xTimeInMs ) * configTICKRATEHZ ) / 1000 ) )
#define pdMS_TO_TICS( xTimeInMs ) ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInMs ) * ( TickType_t ) configTICK_RATE_HZ ) / ( TickType_t ) 1000 ) )

/*******************************************************************************
 * Globals
 ******************************************************************************/
/* Logger queue handle */
static QueueHandle_t log_queue = NULL;
static TaskHandle_t  task_async = NULL;
static  SemaphoreHandle_t xSemaphore = NULL;
static int contador = 0;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* Application API */
static void write_fijo(void *pvParameters);
static void write_variable(void *pvParameters);
//static void write_variable(void *pvParameters);

/* Logger API */
void log_init(uint32_t queue_length, uint32_t max_log_lenght);
static void log_task(void *pvParameters);

static void init_sw2();

/*!
 * @brief Application entry point.
 */
int main(void) {
  /* Init board hardware. */
  BOARD_InitPins();
  BOARD_BootClockRUN();
  BOARD_InitDebugConsole();

  /* Add your code here */
  /* Iniciacion de LEDS */
	// Setup the red LED pin as GPIO
	PORT_SetPinMux(BOARD_LED_RED_GPIO_PORT,
			BOARD_LED_RED_GPIO_PIN,
			kPORT_MuxAsGpio);

	// Setup the blue LED pin as GPIO
	PORT_SetPinMux(BOARD_LED_BLUE_GPIO_PORT,
			BOARD_LED_BLUE_GPIO_PIN,
			kPORT_MuxAsGpio);

	LED_RED_INIT(LOGIC_LED_OFF);
	LED_BLUE_INIT(LOGIC_LED_OFF);

  xSemaphore = xSemaphoreCreateBinary();
  if (xSemaphore != NULL){
	  init_sw2();
	  /* Create RTOS task */
	  log_init(10, sizeof(char *));
	  //vTraceEnable(TRC_START);
	  xTaskCreate(write_fijo, "TAREA_PERIODICA", configMINIMAL_STACK_SIZE + 166, NULL, tskIDLE_PRIORITY + 2, NULL);
	  xTaskCreate(write_variable, "TAREA_APERIODICA", configMINIMAL_STACK_SIZE + 166, NULL, tskIDLE_PRIORITY + 3, NULL);
	  vTaskStartScheduler();

  }

  for(;;) { /* Infinite loop to avoid leaving the main function */
    __asm("NOP"); /* something to use as a breakpoint stop while looping */
  }
}


static void write_fijo(void *pvParameters)
{

	TickType_t xTimeInTicks = pdMS_TO_TICKS( 500 );
	uint32_t i = 0;
	for (;;)
	{

		char * log =  pvPortMalloc( sizeof(char *) * strlen("[PERIODIC TASK]"));
		sprintf(log, "[PERIODIC TASK]");
		xQueueSend(log_queue, &log, 0);
		LED_RED_TOGGLE();
		vTaskDelay(xTimeInTicks);

	}
	//vTaskSuspend(NULL);
}

static void write_variable(void *pvParameters){
	uint32_t counter = 1;
	 for( ;; )
	 {
		 //Intento tomar el semaforo y me bloqueo hasta que llegue la interrupcion
		 xSemaphoreTake( xSemaphore, portMAX_DELAY );
		 char * log =  pvPortMalloc( sizeof(char *) * counter);
		 sprintf(log,"*");
		 for (int i = 0; i< counter-1; i++){
			 strcat(log,"*");
		 }
		 xQueueSend(log_queue, &log, 0);
		 LED_BLUE_TOGGLE();
		 counter++;
	 }

}

void log_init(uint32_t queue_length, uint32_t max_log_lenght)
{
	log_queue = xQueueCreate(queue_length, max_log_lenght);
	//Crea la tarea que imprime con prioridad baja
	xTaskCreate(log_task, "printer_task", configMINIMAL_STACK_SIZE + 166, NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void log_task(void *pvParameters)
{
	uint32_t counter = 0;
	//	char log[MAX_LOG_LENGTH + 1];
	char * log;
	while (1)
	{
		xQueueReceive(log_queue, &log, portMAX_DELAY);
		//vPrintString(log);
		PRINTF("Log %d: %s\r\n", counter, log);
		vPortFree(log);
		counter++;
	}
}

static void init_sw2()
{
	// Enable the clock to the PORT module that the switch is on.
	CLOCK_EnableClock(kCLOCK_PortC);

	// Setup the SW2 pin as GPIO
	PORT_SetPinMux(BOARD_SW2_PORT,
			BOARD_SW2_GPIO_PIN,
			kPORT_MuxAsGpio);

	// Setup the SW2 pin interrupt
	PORT_SetPinInterruptConfig(BOARD_SW2_PORT, BOARD_SW2_GPIO_PIN, kPORT_InterruptFallingEdge);
	NVIC_SetPriority(BOARD_SW2_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
	EnableIRQ(BOARD_SW2_IRQ);

	// Setup the GPIO pin to be an input
	GPIO_PinInit(BOARD_SW2_GPIO, BOARD_SW2_GPIO_PIN, &(gpio_pin_config_t){kGPIO_DigitalInput, 0});
}

void BOARD_SW2_IRQ_HANDLER()
{
	PORT_ClearPinsInterruptFlags(BOARD_SW2_PORT, 1<<BOARD_SW2_GPIO_PIN);
	uint32_t result = GPIO_ReadPinInput(BOARD_SW2_GPIO,BOARD_SW2_GPIO_PIN); // Leo el estado del boton
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );  //Se pone en true xHiguer cuando se desbloquea la tarea
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken ); //Cambio de contecto

}



