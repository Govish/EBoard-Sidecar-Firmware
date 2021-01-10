#ifndef BUZZER_H
#define BUZZER_H

#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim2; //structure to manipulate the timer 2 settings

//initialize the necessary stuff for the buzzer
void buzzer_init();

//some buzz/alert routines
void buzz_done_init();
void buzz_warn_low();
void buzz_warn_critical();

//get the free stack space of the relevant buzzer thread
uint32_t buzzer_stack_space();

#endif
