#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "buzzer.h"
#include "cmsis_os.h"

extern osThreadId_t StateMachineHandle;
extern TIM_HandleTypeDef htim2;

//basically our main code goes here
void doStateMachine(void *argument) {
	buzzer_init();
	osDelay(100);
	buzz_done_init();
	osDelay(1000);

	buzz_warn_low();

	osDelay(3000);

	buzz_warn_critical();
	osDelay(10000);

	uint32_t buzz_stack;
	buzz_stack = buzzer_stack_space();

	while(1) {
		osDelay(1);
	}
	//should never get here but just sanity checking
	osThreadExit();
}

#endif
