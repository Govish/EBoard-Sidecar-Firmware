#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "cmsis_os.h"
#include "pushbutton.h"
#include "stdbool.h"

extern osThreadId_t StateMachineHandle;
extern TIM_HandleTypeDef htim2;
extern osEventFlagsId_t pushbutton_flags;

//basically our main code goes here
void doStateMachine(void *argument) {
	uint32_t tick1, tick2;

	pushbutton_init();
	tick1 = HAL_GetTick();

	osEventFlagsWait(pushbutton_flags, BUTTON_RELEASED, osFlagsWaitAny, osWaitForever); //make sure the button has a valid state
	osEventFlagsClear(pushbutton_flags, BUTTON_FLAGS_ALL); //clear all button flags, mostly the long press flag

	osEventFlagsWait(pushbutton_flags, BUTTON_LONG_PRESSED, osFlagsWaitAny, osWaitForever); //precharge for 5 seconds
	HAL_GPIO_WritePin(FET_DRV_GPIO_Port, FET_DRV_Pin, GPIO_PIN_SET); //enable the high side FETs

	pushbutton_led_on();
	osDelay(2000);
	pushbutton_led_off();
	osDelay(2000);
	pushbutton_led_flash();
	osDelay(5000);
	pushbutton_led_on();
	osDelay(2000);
	pushbutton_led_fade();
	osDelay(5000);
	tick2 = HAL_GetTick();

	uint32_t stack;
	stack = led_stack_space();
	stack = pushbutton_stack_space();

	pushbutton_led_off();
	osEventFlagsClear(pushbutton_flags, BUTTON_FLAGS_ALL);
	while(1) {
		HAL_GPIO_TogglePin(CC_CHAN_1_GPIO_Port, CC_CHAN_1_Pin);
		osDelay(1000);

		/*
		osEventFlagsWait(pushbutton_flags, BUTTON_BUMPED | BUTTON_SHORT_PRESSED | BUTTON_LONG_PRESSED,
				osFlagsNoClear, osWaitForever);
		if(pushbutton_short_pressed(true)) pushbutton_led_on();
		else if(pushbutton_long_pressed(true)) pushbutton_led_flash();
		else if(pushbutton_bumped(true)) pushbutton_led_fade();
		*/
	}
	//should never get here but just sanity checking
	osThreadExit();
}

#endif
