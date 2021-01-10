#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "bargraph.h"
#include "cmsis_os.h"

extern osThreadId_t StateMachineHandle;

//basically our main code goes here
void doStateMachine(void *argument) {
	bargraph_init();

	for(float i = 0; i < 1; i+= 0.05) {
		bargraph_draw_soc(i);
		osDelay(7500);
	}

	uint32_t draw_space, main_space, animate_space;
	draw_space = bargraph_draw_stack_space();
	animate_space = bargraph_animate_stack_space();
	main_space = osThreadGetStackSpace(StateMachineHandle);

	while(1) {
		osDelay(1);
	}
	//should never get here but just sanity checking
	osThreadExit();
}

#endif
