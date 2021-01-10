#pragma once

#include "stm32f4xx_hal.h"
#include "main.h"

//simple wrapper header file for the ports and pins

//set the bargraph LED pins according to the value passed into this function
//similar to the busOut() interface in MBed
void bargraph_output(uint16_t states) {
	HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, (states & 1<<0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, (states & 1<<1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, (states & 1<<2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, (states & 1<<3) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, (states & 1<<4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, (states & 1<<5) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, (states & 1<<6) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, (states & 1<<7) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, (states & 1<<8) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED9_GPIO_Port, LED9_Pin, (states & 1<<9) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

