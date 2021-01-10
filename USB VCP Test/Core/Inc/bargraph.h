#ifndef BARGRAPH_H
#define BARGRAPH_H

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

//initialize the threads for the LED bargraph
//pass the message queue ID of the SOC buffer
void bargraph_init(osMessageQueueId_t soc_buf_id);

//draw a particular SOC on the bargraph display
void bargraph_draw_soc();

//get the free stack space of the relevant drawing thread
uint32_t bargraph_draw_stack_space();
uint32_t bargraph_animate_stack_space();


#endif
