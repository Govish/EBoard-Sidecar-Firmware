#ifndef BARGRAPH_H
#define BARGRAPH_H

#include "stm32f4xx_hal.h"

//initialize the threads for the LED bargraph
void bargraph_init();

//draw a particular SOC on the bargraph display
void bargraph_draw_soc(float soc);

//get the free stack space of the relevant drawing thread
uint32_t bargraph_draw_stack_space();
uint32_t bargraph_animate_stack_space();


#endif
