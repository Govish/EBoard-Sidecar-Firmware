#include "pushbutton.h"
#include "cmsis_os.h"
#include "main.h" //for pin mappings

//================ SOME DEFINES ==================
#define BUTTON_BOUNCE_TIME 25 //sets the speed of the button update thread

#define PWM_PERIOD 999ul //for normal PWM operation
#define BLINK_PERIOD 333333ul //for LED blink operation
#define COUNT_STEP 25 //PWM counter step during fading
#define FADE_DELAY 10 //how quickly the PWM counter should increment/decrement

//bit fields for the LED action flags
#define BIT_LED_ON (1<<0)
#define BIT_LED_OFF (1<<1)
#define BIT_LED_FADE (1<<2)
#define BIT_LED_FLASH (1<<3)
#define BIT_LED_ALL 0x0F

//============= PRIVATE VARIABLES =============
static osEventFlagsId_t led_action_flags;

static osThreadId_t led_thread_handle = NULL;
static osThreadId_t button_thread_handle = NULL;

//============= PRIVATE FUNCTION PROTOTYPES ==============
//thread functions for the LED and button sampling thread
static void led_thread(void* argument);
static void button_thread(void* argument);

static void do_led_on();
static void do_led_off();
static void do_led_fade();
static void do_led_flash();


//============= PUBLIC FUNCTION DEFINITIONS =============
osEventFlagsId_t pushbutton_init() {
	//initialize the signal flags for the LED thread and the pushbutton status flags
	led_action_flags = osEventFlagsNew(NULL);
	pushbutton_flags = osEventFlagsNew(NULL);

	//initialize the led thread
	const osThreadAttr_t led_attributes = {
			.name = "led",
			.priority = (osPriority_t) osPriorityNormal,
			.stack_size = 288
	};
	led_thread_handle = osThreadNew(led_thread, NULL, &led_attributes);

	//initialize the main button thread
	const osThreadAttr_t button_attributes = {
			.name = "button",
			.priority = (osPriority_t) osPriorityAboveNormal,
			.stack_size = 256
	};
	button_thread_handle = osThreadNew(button_thread, NULL, &button_attributes);

	//return the ID of the pushbutton event flags
	return pushbutton_flags;
}

void pushbutton_led_on() {osEventFlagsSet(led_action_flags, BIT_LED_ON); }
void pushbutton_led_off() {osEventFlagsSet(led_action_flags, BIT_LED_OFF); }
void pushbutton_led_fade() {osEventFlagsSet(led_action_flags, BIT_LED_FADE); }
void pushbutton_led_flash() {osEventFlagsSet(led_action_flags, BIT_LED_FLASH); }

bool pushbutton_released(bool clear_flag) {
	bool result = osEventFlagsGet(pushbutton_flags) & BUTTON_RELEASED;
	if(result && clear_flag) osEventFlagsClear(pushbutton_flags, BUTTON_RELEASED);
	return result;
}

bool pushbutton_bumped(bool clear_flag) {
	bool result = osEventFlagsGet(pushbutton_flags) & BUTTON_BUMPED;
	if(result && clear_flag) osEventFlagsClear(pushbutton_flags, BUTTON_BUMPED);
	return result;
}

bool pushbutton_short_pressed(bool clear_flag) {
	bool result = osEventFlagsGet(pushbutton_flags) & BUTTON_SHORT_PRESSED;
	if(result && clear_flag) osEventFlagsClear(pushbutton_flags, BUTTON_SHORT_PRESSED);
	return result;
}

bool pushbutton_long_pressed(bool clear_flag) {
	bool result = osEventFlagsGet(pushbutton_flags) & BUTTON_LONG_PRESSED;
	if(result && clear_flag) osEventFlagsClear(pushbutton_flags, BUTTON_LONG_PRESSED);
	return result;
}

uint32_t pushbutton_stack_space() {
	return osThreadGetStackSpace(button_thread_handle);
}

uint32_t led_stack_space() {
	return osThreadGetStackSpace(led_thread_handle);
}

//====================== PRIVATE FUNCTION DEFINITIONS ======================
static void button_thread(void* argument) {
	GPIO_PinState last_state;
	bool last_button; //high equates to the button currently being pressed
	bool short_press, long_press; //flags that tell us if we've set the appropriate event flags yet
	uint32_t push_time; //timestamp of when the button was pressed

	while(1) {
		//button pressed -> gpio state will be high
		GPIO_PinState current_button_state = HAL_GPIO_ReadPin(PB_IN_GPIO_Port, PB_IN_Pin);

		//if the button has settled, record the state
		if(current_button_state == last_state) {

			if(current_button_state == GPIO_PIN_SET) { //button is pressed
				if(!last_button) push_time = HAL_GetTick(); //store the time if it was just pressed

				//check for short or long presses
				if(!short_press && ((HAL_GetTick() - push_time) > BUTTON_SHORT_PRESS_TIME)) {
					osEventFlagsSet(pushbutton_flags, BUTTON_SHORT_PRESSED);
					short_press = true;
				}
				if(!long_press && ((HAL_GetTick() - push_time) > BUTTON_LONG_PRESS_TIME)) {
					osEventFlagsSet(pushbutton_flags, BUTTON_LONG_PRESSED);
					long_press = true;
				}

				//remember the button state
				last_button = true;
			}

			else { //button is released
				if(last_button) {//button was just released
					//if the button was pressed for less than the "short press time"
					if((HAL_GetTick() - push_time) < BUTTON_SHORT_PRESS_TIME)
						osEventFlagsSet(pushbutton_flags, BUTTON_BUMPED); //set the event flag for a "bump"

					//also set the event flag for releasing the button
					osEventFlagsSet(pushbutton_flags, BUTTON_RELEASED);

					//reset some of our local flags for short and long presses
					short_press = false;
					long_press = false;
				}
				last_button = false;
			}

		}

		last_state = current_button_state; //updating the debounce memory (pin state)
		osDelay(BUTTON_BOUNCE_TIME);
	}
}

static void led_thread(void* argument) {
	HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1); //start the PWM timer for the LED

	while(1) {
		//suspend this thread until any flag is set (and don't clear the set flag)
		osEventFlagsWait(led_action_flags, BIT_LED_ALL, osFlagsNoClear, osWaitForever);

		//if a flag bit is set, run the appropriate LED routine
		//order of the conditionals indicates the priority of the flash routines
		//each routine will return pretty much immediately though
		if(osEventFlagsGet(led_action_flags) & BIT_LED_FLASH) {
			do_led_flash();
			osEventFlagsClear(led_action_flags, BIT_LED_FLASH); //clear the flag manually
		}
		else if (osEventFlagsGet(led_action_flags) & BIT_LED_FADE) {
			osEventFlagsClear(led_action_flags, BIT_LED_FADE); //clearing this before so the break function works
			do_led_fade();
		}
		else if (osEventFlagsGet(led_action_flags) & BIT_LED_ON) {
			do_led_on();
			osEventFlagsClear(led_action_flags, BIT_LED_ON);
		}
		else if (osEventFlagsGet(led_action_flags) & BIT_LED_OFF) {
			do_led_off();
			osEventFlagsClear(led_action_flags, BIT_LED_OFF);
		}
	}
	//gracefully exiting if it somehow gets here
	osThreadExit();
}

//helper functions for the LED threads
static void do_led_on() {
	htim5.Instance->CCR1 = UINT32_MAX; //just max out counter register to force the channel on
}

static void do_led_off() {
	htim5.Instance->CCR1 = 0; //set counter register to zero to force the channel off
}

static void do_led_fade() {
	//reset the LED PWM peripheral to normal PWM mode
	//necessary if called after configuring peripheral for flashing the LED
	htim5.Init.Period = PWM_PERIOD;
	htim5.Instance->CCR1 = 0;
	HAL_TIM_PWM_Init(&htim5);

	//have a local variable that sets the increment/decrement amount
	int count_amount;

	//run the fade forever until we want to swap to a different event
	while(1) {
		//set the direction that we're gonna be counting
		if(htim5.Instance->CCR1 > 1000) count_amount = -COUNT_STEP;
		else if (htim5.Instance->CCR1 < COUNT_STEP) count_amount = COUNT_STEP;

		htim5.Instance->CCR1 += count_amount; //increment compare value for the PWM module
		osDelay(FADE_DELAY);

		//exit the loop if there's any of event flag that's asserted
		if(osEventFlagsGet(led_action_flags)) break;
	}
}

static void do_led_flash() {
	htim5.Init.Period = BLINK_PERIOD; //slow the PWM timer waaaaay down
	htim5.Instance->CCR1 = BLINK_PERIOD>>1; //set the duty cycle to 50%
	HAL_TIM_PWM_Init(&htim5);
}
