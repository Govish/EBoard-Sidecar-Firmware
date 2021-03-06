#ifndef BOARD_LIGHTS_H
#define BOARD_LIGHTS_H

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

extern TIM_HandleTypeDef htim4; //structure to manipulate the timer 4 settings

//initialize the threads for the headlights and taillights
void board_lights_init(TIM_HandleTypeDef* h);

//shutdown the headlights gracefully
void board_lights_shutdown();

//called on pulse timer overflow
void board_lights_tim_overflow();

uint32_t lights_super_stack_space();
uint32_t lights_anim_stack_space();

#endif
