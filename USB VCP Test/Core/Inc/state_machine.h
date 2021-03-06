#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "cmsis_os.h"
#include "pushbutton.h"
#include "batt_monitor.h"
#include "buzzer.h"
#include "bargraph.h"
#include "stdbool.h"
#include "printf_override.h"

//extern osThreadId_t StateMachineHandle;
extern ADC_HandleTypeDef hadc1;

osEventFlagsId_t pb_flags;
osMessageQueueId_t soc_buf;

#define ADC_OVERSAMPLES 16
volatile uint16_t adc_results[ADC_OVERSAMPLES];

void shutdown() {
	//remember to de-init the filesystem
	pushbutton_led_off();
	buzz_shutdown();
	//turn off the headlights and taillights
	HAL_GPIO_WritePin(FET_DRV_GPIO_Port, FET_DRV_Pin, GPIO_PIN_RESET); //logic rail should be enabled long enough to finish buzz
	osDelay(1000);
	HAL_DeInit();
	while(true);
}

//basically our main code goes here
void doStateMachine(void *argument) {
	//initialize the pushbutton "module"
	//and store the pointer to its event flags
	pb_flags = pushbutton_init();
	soc_buf = monitor_init();
	buzzer_init(); //buzz that we've booted and start the buzzer thread
	pushbutton_led_fade(); //fade the LED button on the precharge animation

	osEventFlagsWait(pb_flags, BUTTON_LONG_PRESSED, osFlagsWaitAny, osWaitForever); //precharge for 3 seconds
	if(!v_sys_check(20, &hadc1)) shutdown(); //only start the main thread if the voltage is above 20V

	HAL_GPIO_WritePin(FET_DRV_GPIO_Port, FET_DRV_Pin, GPIO_PIN_SET); //enable the high side FETs to latch power on

	monitor_start(&hadc1); //start the battery monitor
	bargraph_init(soc_buf); //start the bargraph and pass it the ID of the SOC buffer

	buzz_done_init(); //finished all the initialization and fully powered up
	pushbutton_led_on();
	osEventFlagsClear(pb_flags, BUTTON_FLAGS_ALL); //clear all button flags so the board isn't shut down immediately

	uint32_t shutdown_tick = -1; //basically never shut down unless SOC critical or monitor failure
	bool shutdown_latched = false; //flag that says we latched a shutdown signal

	while(1) {
		//do datalogging
		//handle the remote control input
		if(pushbutton_bumped(true)) bargraph_draw_soc(); //report SOC on short button press
		if(pushbutton_short_pressed(true)) pushbutton_led_flash(); //alert the user that a continued hold will shut down the board
		if(pushbutton_long_pressed(true)) shutdown(); //shutdown the board on long-press
		if(pushbutton_released(true)) pushbutton_led_on(); //light the LED solid when the button is released (in case of flashing)

		if(monitor_soc_crit(true) && !shutdown_latched) { //splitting this and the following so we can store separate log messages
			buzz_warn_critical();
			shutdown_tick = HAL_GetTick() + 10000;
			shutdown_latched = true;
		}
		if(monitor_read_fail(true) && !shutdown_latched) {
			buzz_warn_critical();
			shutdown_tick = HAL_GetTick() + 10000;
			shutdown_latched = true;
		}
		if(monitor_soc_low(true)) buzz_warn_low();
		if(HAL_GetTick() > shutdown_tick) shutdown();

		//handle the taillight remote change
	}

	//should never get here but just sanity checking
	osThreadExit();
}

#endif
