#include "board_lights.h"
#include "main.h" //for pin names
#include "stdbool.h"

//================== some defines =====================
#define DEBOUNCER_SAMPLES 5 //how many samples the pulse width debouncer has
#define SUPERVISOR_DELAY 50 //how quickly the supervisor thread runs (osDelay parameter)
#define NUM_FLASH_PATTERNS 4 //how many different flashing patterns there are

#define THRESHOLD_HIGH 	1700 //upper threshold to register a change to "high" re: pulse width
#define THRESHOLD_LOW	1300 //lower threshold to register a change to "low"

#define LIGHTS_OFF 		(1<<0)
#define TAILLIGHT_ONLY 	(1<<1)
#define TAIL_SOLID_HEAD (1<<2)
#define TAIL_AND_HEAD	(1<<3)
#define ALL_LIGHTS_FLAGS (0x0F)

#define CHAN_HEAD_COUNT htim4.Instance->CCR4 //headlights on channel 1
#define CHAN_TAIL_COUNT htim4.Instance->CCR3 //taillights on channel 2

//===================== PRIVATE VARIABLES ========================
static TIM_HandleTypeDef *pulse_tim; //pointer to a timer instance for the RC PWM timer
static osMessageQueueId_t pulse_buf; //a queue that we'll put the pulse width values into
static osEventFlagsId_t flash_flags; //flags to tell the animator thread what pattern to flash

static osThreadId_t lights_sup_handle = NULL; //handle for the board lights "supervisor" thread
static osThreadId_t lights_anim_handle = NULL; //handle for the board lights "animator" thread

//====================== PRIVATE FUNCTION PROTOTYPES ======================
static void run_lights_supervisor(void* argument);
static void run_board_lights(void* argument);

//functions for headlight/taillight flashing
static void do_lights_out();
static void do_taillight_only();
static void do_tail_solid_head();
static void do_tail_and_head();

//====================== PUBLIC FUNCTIONS =========================
//initialize the threads for the headlights and taillights
void board_lights_init(TIM_HandleTypeDef* h) {
	pulse_tim = h;

	flash_flags = osEventFlagsNew(NULL);
	pulse_buf = osMessageQueueNew(1, sizeof(uint16_t), NULL); //creating the pulse width buffer/queue

	//create the "supervisor" thread
	const osThreadAttr_t lights_sup_attributes = {
			.name = "lights supervisor",
			.priority = (osPriority_t) osPriorityBelowNormal,
			.stack_size = 2048
	};
	lights_sup_handle = osThreadNew(run_lights_supervisor, NULL, &lights_sup_attributes); //start the board lights thread

	//create the "animator" thread
	const osThreadAttr_t lights_anim_attributes = {
			.name = "lights animator",
			.priority = (osPriority_t) osPriorityHigh,
			.stack_size = 2048
	};
	lights_anim_handle = osThreadNew(run_board_lights, NULL, &lights_anim_attributes); //start the board lights thread

	//start the RC interrupt and microsecond timer
	HAL_TIM_Base_Start_IT(pulse_tim);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	//start the PWM timers for the constant current drivers
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}

//shutdown the headlights gracefully
void board_lights_shutdown() {
	//kill the lights threads
	if(lights_sup_handle != NULL) osThreadTerminate(lights_sup_handle);
	if(lights_anim_handle != NULL) osThreadTerminate(lights_anim_handle);

	//disable the constant current drivers before power down(just to be gentle to them)
	CHAN_HEAD_COUNT = 0;
	CHAN_TAIL_COUNT = 0;
}

//called on pulse timer overflow
void board_lights_tim_overflow() {
	uint16_t pulse_width = 0; //set the pulse width to 0 on overflow
	osMessageQueueReset(pulse_buf);
	osMessageQueuePut(pulse_buf, &pulse_width, 0, 0);
}

uint32_t board_lights_stack_space() { return osThreadGetStackSpace(lights_sup_handle); }

//===================== PRIVATE/THREAD FUNCTION DEFINITIONS ====================
//lights supervisor function
//controls which lights flashing program to run
static void run_lights_supervisor(void* argument) {
	uint16_t pulse_width;
	uint16_t p_widths[DEBOUNCER_SAMPLES] = {0}; //a simple moving average/debouncer
	uint8_t buf_pointer = 0; //implementing debouncer as a circular buffer
	uint32_t filt_p_width = 0; //variable that holds the sum of the moving averager

	uint8_t which_animation = 0;
	uint8_t timeout_latch = 0;
	uint8_t change_polarity = 1; //1 indicates RISING edge required to change lights

	while(true) {
		//update the pulse width value
		if(osMessageQueueGetCount(pulse_buf) > 0) osMessageQueueGet(pulse_buf, &pulse_width, NULL, 0);
		else pulse_width = 0;

		//save the new pulse width to the debouncer
		p_widths[buf_pointer] = pulse_width;

		//update the moving average sum
		filt_p_width = 0;
		for(int i = 0; i < DEBOUNCER_SAMPLES; i++) filt_p_width += p_widths[i];
		filt_p_width = filt_p_width / DEBOUNCER_SAMPLES;

		//update the buffer pointer
		buf_pointer = (buf_pointer + 1) % DEBOUNCER_SAMPLES;

		if(filt_p_width == 0 && !timeout_latch) {
			osEventFlagsSet(flash_flags, LIGHTS_OFF);
			which_animation = 0;
			timeout_latch = 1;
		}
		else{
			//if the controller button was pressed and the RC input represents that
			//hysteresis for noise reduction
			if( ((filt_p_width > THRESHOLD_HIGH) && change_polarity) ||
				((filt_p_width < THRESHOLD_LOW) && !change_polarity)) {

				//increment the animation that we wanna run and set the appropriate event flag
				which_animation = (which_animation + 1) % NUM_FLASH_PATTERNS;
				osEventFlagsSet(flash_flags, (1<<which_animation));

				change_polarity = !change_polarity;
				timeout_latch = 0;
			}
		}

		osDelay(SUPERVISOR_DELAY);
	}
	osThreadExit();
}

static void run_board_lights(void* argument) {
	while(true) {
		//suspend this thread until any flag is set (and don't clear the set flag)
		osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, osWaitForever);

		//if a flag bit is set, run the appropriate flashing routine
		//order of the conditionals indicates the priority of the flash routines
		if(osEventFlagsGet(flash_flags) & LIGHTS_OFF) {
			osEventFlagsClear(flash_flags, LIGHTS_OFF); //clear the flag manually
			do_lights_out();
		}
		else if (osEventFlagsGet(flash_flags) & TAILLIGHT_ONLY) {
			osEventFlagsClear(flash_flags, TAILLIGHT_ONLY);
			do_taillight_only();
		}
		else if (osEventFlagsGet(flash_flags) & TAIL_SOLID_HEAD) {
			osEventFlagsClear(flash_flags, TAIL_SOLID_HEAD);
			do_tail_solid_head();
		}
		else if (osEventFlagsGet(flash_flags) & TAIL_AND_HEAD) {
			osEventFlagsClear(flash_flags, TAIL_AND_HEAD);
			do_tail_and_head();
		}
	}

	osThreadExit();
}

static void do_lights_out() {
	//set the PWM compare registers to 0
	CHAN_HEAD_COUNT = 0;
	CHAN_TAIL_COUNT = 0;
}

static void do_taillight_only() {
	CHAN_HEAD_COUNT = 0; //turn off headlights

	//infinite loop, but we'll break out of it when any other event flags are asserted
	osStatus_t status;
	while(true) {
		CHAN_TAIL_COUNT = 750;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 925); //non blocking delay with escape
		if(status != osErrorTimeout) return;

		CHAN_TAIL_COUNT = 1000;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 75); //non blocking delay with escape
		if(status != osErrorTimeout) return;
	}
}

static void do_tail_solid_head() {
	CHAN_HEAD_COUNT = 1000; //turn on headlights

	//infinite loop, but we'll break out of it when any other event flags are asserted
	osStatus_t status;
	while(true) {
		CHAN_TAIL_COUNT = 750;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 925); //non blocking delay with escape
		if(status != osErrorTimeout) return;

		CHAN_TAIL_COUNT = 1000;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 75); //non blocking delay with escape
		if(status != osErrorTimeout) return;
	}
}

static void do_tail_and_head() {
	//infinite loop, but we'll break out of it when any other event flags are asserted
	osStatus_t status;
	while(true) {
		CHAN_TAIL_COUNT = 750;
		CHAN_HEAD_COUNT = 1000;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 425); //non blocking delay with escape
		if(status != osErrorTimeout) return;

		CHAN_HEAD_COUNT = 750;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 75); //non blocking delay with escape
		if(status != osErrorTimeout) return;

		CHAN_HEAD_COUNT = 1000;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 425); //non blocking delay with escape
		if(status != osErrorTimeout) return;

		CHAN_HEAD_COUNT = 750;
		CHAN_TAIL_COUNT = 1000;
		status = osEventFlagsWait(flash_flags, ALL_LIGHTS_FLAGS, osFlagsNoClear, 75); //non blocking delay with escape
		if(status != osErrorTimeout) return;
	}
}


//============================ ISRs ===========================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == RC_IN_Pin) {
		//if the pin is high, reset the timer
		if(HAL_GPIO_ReadPin(RC_IN_GPIO_Port, RC_IN_Pin) == GPIO_PIN_SET) {
			pulse_tim->Instance->CNT = 0; //reset the timer
		}

		//if the pin is low, check to see if it's valid and grab the pulse width
		else {
			uint16_t pulse_width;
			pulse_width = pulse_tim->Instance->CNT;
			osMessageQueueReset(pulse_buf);
			osMessageQueuePut(pulse_buf, &pulse_width, 0, 0);
		}
	}
}

