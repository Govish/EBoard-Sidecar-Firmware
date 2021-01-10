#include "buzzer.h"
#include "cmsis_os.h"


//================ SOME DEFINES ==================
//bit locations for the event flags to signal the buzzer main thread
#define BIT_BOOT_UP 	(1<<0)
#define BIT_DONE_INIT 	(1<<1)
#define BIT_WARN_LOW 	(1<<2)
#define BIT_WARN_CRIT	(1<<3)
#define BIT_SHUTDOWN 	(1<<4)
#define BIT_ALL_FLAGS 	0x1F

//macro for setting the timer period
#define TIME_PERIOD(period) { \
	htim2.Init.Period = period; \
	HAL_TIM_Base_Init(&htim2); \
}

//quick macros to start and stop the timer
#define START() HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_3)
#define STOP() HAL_TIM_OC_Stop(&htim2, TIM_CHANNEL_3)

#define BOOT_BUZZ_DELAY 150
#define INIT_DONE_DELAY 50
#define INIT_DONE_PAUSE 150
#define WARN_BUZZ_TIME 75
#define WARN_OFF_TIME 250
#define CRITIAL_ON_TIME 900
#define CRITICAL_OFF_TIME 100

//============= PRIVATE VARIABLES =============
//thread handle for the buzzer thread
static osThreadId_t buzzer_handle = NULL;

//flags to tell the buzzer thread what action to run
static osEventFlagsId_t buzzer_action_flags = NULL;

//============= PRIVATE FUNCTION PROTOTYPES ==============
static void buzzer_thread(void* argument); //main buzzer thread function

static void do_buzz_boot_up();
static void do_buzz_done_init();
static void do_buzz_warn_low();
static void do_buzz_warn_critical();
static void do_buzz_shutdown();

//============= PUBLIC FUNCTION DEFINITIONS =============
void buzzer_init() {
	//initialize the signal flags for the buzzer thread
	buzzer_action_flags= osEventFlagsNew(NULL);
	osEventFlagsSet(buzzer_action_flags, BIT_BOOT_UP); //set the boot up bit right away

	//initialize the main buzzer thread
	const osThreadAttr_t buzzer_atributes = {
			.name = "buzzer",
			.priority = (osPriority_t) osPriorityAboveNormal,
			.stack_size = 350 //possibly shrink this a little
	};
	buzzer_handle = osThreadNew(buzzer_thread, NULL, &buzzer_atributes);
}

//just set the action flag for the buzzer thread and return
void buzz_done_init() { osEventFlagsSet(buzzer_action_flags, BIT_DONE_INIT); }
void buzz_warn_low() {osEventFlagsSet(buzzer_action_flags, BIT_WARN_LOW); }
void buzz_warn_critical() {osEventFlagsSet(buzzer_action_flags, BIT_WARN_CRIT); }
void buzz_shutdown() {osEventFlagsSet(buzzer_action_flags, BIT_SHUTDOWN); }

//return the free stack space for the buzzer thread
uint32_t buzzer_stack_space() {return osThreadGetStackSpace(buzzer_handle);}


//====================== PRIVATE FUNCTION DEFINITIONS ======================

//buzzer main thread function
static void buzzer_thread(void* argument) {
	while(1) {
		//suspend this thread until any flag is set (and don't clear the set flag)
		osEventFlagsWait(buzzer_action_flags, BIT_ALL_FLAGS, osFlagsNoClear, osWaitForever);

		//if a flag bit is set, run the appropriate buzzer routine
		//order of the conditionals indicates the priority of the buzz routines
		//each routine will fully complete before going to another routine
		if(osEventFlagsGet(buzzer_action_flags) & BIT_WARN_CRIT) {
			do_buzz_warn_critical();
			osEventFlagsClear(buzzer_action_flags, BIT_WARN_CRIT); //clear the flag manually
		}
		else if (osEventFlagsGet(buzzer_action_flags) & BIT_WARN_LOW) {
			do_buzz_warn_low();
			osEventFlagsClear(buzzer_action_flags, BIT_WARN_LOW);
		}
		else if (osEventFlagsGet(buzzer_action_flags) & BIT_DONE_INIT) {
			do_buzz_done_init();
			osEventFlagsClear(buzzer_action_flags, BIT_DONE_INIT);
		}
		else if (osEventFlagsGet(buzzer_action_flags) & BIT_BOOT_UP) {
			do_buzz_boot_up();
			osEventFlagsClear(buzzer_action_flags, BIT_BOOT_UP);
		}
		else if (osEventFlagsGet(buzzer_action_flags) & BIT_SHUTDOWN) {
			do_buzz_shutdown();
			osEventFlagsClear(buzzer_action_flags, BIT_SHUTDOWN);
		}
	}
	//gracefully exiting if it somehow gets here
	osThreadExit();
}

static void do_buzz_boot_up() {
	TIME_PERIOD(1000);
	START();
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(800); //major 3rd from base
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(666); //perfect 5th from base
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(500); //perfect octave from base
	osDelay(BOOT_BUZZ_DELAY);

	STOP();
	osDelay(250); //chill for a bit before returning
}

static void do_buzz_done_init() {
	for(int i = 0; i < 1; i++) {
		TIME_PERIOD(1000);
		START();
		osDelay(INIT_DONE_DELAY);

		TIME_PERIOD(800); //major third above base note
		osDelay(INIT_DONE_DELAY);

		TIME_PERIOD(666); //perfect fifth from base note
		osDelay(INIT_DONE_DELAY);

		TIME_PERIOD(500); //octave from base note
		osDelay(INIT_DONE_DELAY);

		STOP();
		osDelay(INIT_DONE_PAUSE);
	}
}

static void do_buzz_warn_low() {
	for(int i = 0; i < 8; i++) {
		TIME_PERIOD(125);
		START();
		osDelay(WARN_BUZZ_TIME);
		TIME_PERIOD(188);
		osDelay(WARN_BUZZ_TIME);
		TIME_PERIOD(250);
		osDelay(WARN_BUZZ_TIME);
		STOP();

		osDelay(WARN_OFF_TIME);
	}

}

static void do_buzz_warn_critical() {
	for(int i = 0; i < 8; i++) {
		TIME_PERIOD(125); //125 before, making 250 to make testing less annoying
		START();
		osDelay(CRITIAL_ON_TIME);

		STOP();
		osDelay(CRITICAL_OFF_TIME);
	}
}

static void do_buzz_shutdown() {
	TIME_PERIOD(500);
	START();
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(666); //major 3rd from base
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(800); //perfect 5th from base
	osDelay(BOOT_BUZZ_DELAY);

	TIME_PERIOD(1000); //perfect octave from base
	osDelay(BOOT_BUZZ_DELAY);

	STOP();
	osDelay(250); //chill for a bit before returning
}
