#ifndef PUSHBUTTON_H
#define PUSHBUTTON_H

#include "stm32f4xx_hal.h"
#include "stdbool.h"
#include "cmsis_os.h"

#define BUTTON_SHORT_PRESS_TIME 1000 //ms registered as a short press
#define BUTTON_LONG_PRESS_TIME 3000 //ms registered as a long press

#define BUTTON_RELEASED (1<<0)
#define BUTTON_BUMPED (1<<1) //button pressed and released before LONG_PRESS_TIME elapses
#define BUTTON_SHORT_PRESSED (1<<2) //button pushed in for longer than SHORT_PRESS_TIME
#define BUTTON_LONG_PRESSED (1<<3) //button pushed in for longer than LONG_PRESS_TIME
#define BUTTON_FLAGS_ALL 0x0F


//flags to alert other threads about the pushbutton status
osEventFlagsId_t pushbutton_flags;

//timer handle for LED PWM control
extern TIM_HandleTypeDef htim5;

//initialize necessary stuff for the pushbutton threads
//returns the ID of the pushbutton event flags
osEventFlagsId_t pushbutton_init();

//=========== some functions to make reading/clearing button flags easy ===========
bool pushbutton_released(bool clear_flag);
bool pushbutton_bumped(bool clear_flag);
bool pushbutton_short_pressed(bool clear_flag);
bool pushbutton_long_pressed(bool clear_flag);
//=================================================================================

//get the stack space that the various threads are using
uint32_t pushbutton_stack_space();
uint32_t led_stack_space();

//============== LED Control stuff ============
void pushbutton_led_on();
void pushbutton_led_off();
void pushbutton_led_fade();
void pushbutton_led_flash();

#endif
