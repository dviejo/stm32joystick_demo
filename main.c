/* HID Joystic emulation for stm32f103C8 (bluepill)
 *	Demo program
 *  Created by Diego Viejo dviejo@ua.es for educational purposes
 *  on May the fourth 2020 ;)
 * 
 * 
 * Uses libOpenCM3 HAL Library and FreeRTOS
 * 		Thanks to Warren Gay and his "Beginning STM32: Developing with FreeRTOS, libopencm3 and GCC"
 * 
 * I've picked up ideas from these (thanks a lot for sharing your knowledge):
 * 	- https://satoshinm.github.io/blog/171227_stm32hid_pill_duck_scriptable_usb_hid_device_using_an_stm32_blue_pill_from_mouse_jigglers_to_rubber_duckies.html
 *  - https://hackaday.io/project/163715-wii-nunchuk-as-a-usb-hid-controller/log/159261-first-steps-with-stm32-usb-firmware
 * 
 * 
 * The bluepill is configured as a multiaxis joystick with:
 * 	- 8 buttons
 * 	- 6 axis: xAxis, yAxis, zAxis, Accelerator, Brake, Steering
 * 
 * Buttons are pressed every 500ms in a sequence, then released in a sequence too, and so on.
 * xAxis is moved back and forth
 * 
 * This firmware has been flashed in a stm32f103C8 Bluepill device and tested using linux jstest program.
 * 
 */

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>


#include "usbhid.h"
#include "joystick.h"

#define mainECHO_TASK_PRIORITY				( tskIDLE_PRIORITY + 1 )

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName);

// Communication queue
static QueueHandle_t joystick_txq;

// instance of Joystick
static 	struct Joystick_ joystick;

void
vApplicationStackOverflowHook(xTaskHandle *pxTask __attribute((unused)),signed portCHAR *pcTaskName __attribute((unused))) {
	for(;;);
}

static void
gpio_setup(void) {

	rcc_clock_setup_in_hse_8mhz_out_72mhz();	// Use this for "blue pill"


	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
}

/**
 * xAxis demo task
 */
static void
axis_demo_task(void *args __attribute((unused))) {
	int value = 0;
	int increment = 10;

	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(100));
		if(value > JOYSTICK_DEFAULT_AXIS_MAXIMUM || value < JOYSTICK_DEFAULT_AXIS_MINIMUM)
			increment = -increment;

		value += increment;

		Joystick_setXAxis(&joystick, value);
	}
}


/**
 * buttons demo task
 */
static void buttons_demo_task(void *arg __attribute((unused)))
{
	int8_t btn = 1;
	int8_t next = 1;

	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(500));
		if(!(~btn))
		{
			next = 0;
		}
		if(!btn) next = 1;
		btn = (btn<<1) | next;

		Joystick_setButtons(&joystick, btn);
	}
}

int
main(void) {

	joystick_txq = xQueueCreate(128,sizeof(char));

	gpio_setup();
	
	usbhid_start(&joystick_txq);

	//joystick init
	Joystick_start(&joystick, &joystick_txq);

	xTaskCreate(axis_demo_task,"xAxis",50,NULL,configMAX_PRIORITIES-1,NULL);
	xTaskCreate(buttons_demo_task,"Buttons",50,NULL,configMAX_PRIORITIES-1,NULL);


	vTaskStartScheduler();
	for (;;)
		;
	return 0;
}

// End
