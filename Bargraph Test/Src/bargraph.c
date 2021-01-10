#include "bargraph.h"
#include "stdbool.h"
#include "cmsis_os.h"
#include "pindefs.h"

//================== some defines =====================
//how many ticks (ms) we need to wait between the LED muxing
#define BARGRAPH_UPDATE_DELAY 2
#define ANIMATOR_READY (1<<0) //flag bit that is cleared with the animator is running

//animation-related defines
#define CRITICAL_FLASH_RATE 75 //tells us how quickly to flash the bottom LED if the SOC is "critical"
#define CRITICAL_FLASH_COUNT 10 //how many times to flash the bottom LED in a critical soc
#define BUILDUP_DELAY 50 //tells us how long the successive LEDs of the bar graph are built up for
#define FLASH_DELAY 500 //tells us half the period of the SOC flashing
#define FLASH_COUNT 4

//macro for updating the queues;
#define Q_UPDATE(queue_name, data) 	{ \
	osMessageQueueReset(queue_name); \
	osMessageQueuePut(queue_name, &data, 0, 0); \
}
//===================== PRIVATE VARIABLES ========================
static osMessageQueueId_t drawbuf_queue; //reference to the message queue for the draw buffer
static osMessageQueueId_t socbuf_queue; //reference to the message queue to pass SOC to the animate thread

static osEventFlagsId_t animator_run_flags; //flags to notify status of animation thread (and whether it's ready)

//handles to deal with the animation and the drawing threads
static osThreadId_t drawHandle = NULL;
static osThreadId_t animatorHandle = NULL;

//====================== PRIVATE FUNCTION PROTOTYPES ======================
static void draw_bargraph(void *argument); //runs in a thread context
static void animate_bargraph(void *argument); //runs in a thread context

//====================== PUBLIC FUNCTIONS =========================
void bargraph_init() {
	//create the bargraph drawing thread
	const osThreadAttr_t draw_attributes = {
			.name = "draw",
			.priority = (osPriority_t) osPriorityHigh,
			.stack_size = 256
	};
	//suspends itself right at startup
	drawHandle = osThreadNew(draw_bargraph, NULL, &draw_attributes);

	//create the queue to send the bytes to the draw buffer
	//just need one element to ferry the new "image" over to the thread safely
	drawbuf_queue = osMessageQueueNew(1, sizeof(uint16_t), NULL);

	//start a new animation thread too
	//this thread immediately suspends itself until it gets resumed by the draw_soc function
	const osThreadAttr_t animator_attributes = {
			.name = "animator",
			.priority = (osPriority_t) osPriorityAboveNormal,
			.stack_size = 448
	};
	animatorHandle = osThreadNew(animate_bargraph, NULL, &animator_attributes);

	//create the queue to send the SOC to the animation thread
	//just nead one element to ferry the SOC to render to the thread safely
	socbuf_queue = osMessageQueueNew(1, sizeof(float), NULL);

	//create the flags to notify the status of the animation thread
	//want to tell the "draw_soc" function whether the animtator is running or not
	animator_run_flags = osEventFlagsNew(NULL);
}

//draw a particular SOC on the bargraph display
void bargraph_draw_soc(float soc) {
	//only restart the animation when the animator is suspended
	if(osEventFlagsGet(animator_run_flags) & ANIMATOR_READY) {
		osStatus_t status;
		Q_UPDATE(socbuf_queue, soc);
		status = osThreadResume(animatorHandle);
		UNUSED(status);
	}
}

//report free stack space for the draw thread
uint32_t bargraph_draw_stack_space() {
	return osThreadGetStackSpace(drawHandle);
}

uint32_t bargraph_animate_stack_space() {
	return osThreadGetStackSpace(animatorHandle);
}

//===================== PRIVATE/THREAD FUNCTION DEFINITIONS ====================

//draw the SOC animation on the LED bargraph
//gets called in a thread context
void animate_bargraph(void* argument) {
	float soc = 0;

	while(true) {
		//suspend this thread on startup
		osEventFlagsSet(animator_run_flags, ANIMATOR_READY); //ready to re-run animation
		osThreadSuspend(animatorHandle);

		//============= resuming the animation thread ==========
		uint16_t display_buffer = 0;
		osEventFlagsClear(animator_run_flags, ANIMATOR_READY); //running animation, clear flags
		//pull the SOC from the queue
		if(osMessageQueueGetCount(socbuf_queue) > 0) {
			osStatus_t status; //storing status for debugging purposes
			status = osMessageQueueGet(socbuf_queue, &soc, NULL, 0);
			UNUSED(status);
		}
		Q_UPDATE(drawbuf_queue, display_buffer); //start with drawing nothing
		osThreadResume(drawHandle); //restart the draw thread

		//================= running the animation =================
		soc = soc >= 1 ? 0.999 : (soc < 0 ? 0 : soc); //constraining SOC between 0 and 1 exclusive
		//gives us an integer version of the SOC that we can work with to draw the bargraph
		//a "solid" light will denote a x6-10 SOC
		//i.e. a full SOC (96-100%) will have all lights lit
		//and a 54% SOC will have the first 5 lights be solid and the 6th light blinking
		uint8_t scaled_soc = (uint8_t)(soc*20);

		//flash the bottom most LED if the SOC is "critical"
		//involves updating display buffer, clearing the queue, and pushing it into the queue
		if(scaled_soc < 1) {
			for(int i = 0; i < CRITICAL_FLASH_COUNT; i++) {
				display_buffer = 1;
				Q_UPDATE(drawbuf_queue, display_buffer);
				osDelay(CRITICAL_FLASH_RATE);

				display_buffer = 0;
				Q_UPDATE(drawbuf_queue, display_buffer);
				osDelay(CRITICAL_FLASH_RATE);
			}
		}

		//if the SOC is greater than 5%
		else {
			//draw an animation to light up all the "solid lights" before the last one
			display_buffer = 0;
			for(int i = 0; i < (scaled_soc>>1); i++) {
				display_buffer |= (1<<i);
				Q_UPDATE(drawbuf_queue, display_buffer);
				osDelay(BUILDUP_DELAY);
			}

			//if the top number is odd, then make the LED solid
			if(scaled_soc & 0x01) {
				display_buffer |= (1 << (scaled_soc >> 1)); //add the extra LED lit up
				Q_UPDATE(drawbuf_queue, display_buffer);
				osDelay(FLASH_DELAY * FLASH_COUNT * 2);
			}

			//if the scaled SOC is even, flash the top LED
			else {
				for(int i = 0; i < FLASH_COUNT<<1; i++) {
					display_buffer ^= 1 << (scaled_soc>>1); //toggle this particular bit in the buffer
					Q_UPDATE(drawbuf_queue, display_buffer);
					osDelay(FLASH_DELAY);
				}
			}
		}

		//====== shutting the draw thread down ======
		osThreadSuspend(drawHandle); //suspend the drawing thread since we're done animating
		bargraph_output(0); //clear the bargraph pins
	}

	//exit the current thread gracefully if it somehow gets here
	osThreadExit();
}


//draws the bargraph
//gets called in a thread context
//@param argument: doesn't matter, not used
static void draw_bargraph(void *argument) {
	uint16_t draw_buffer = 0;
	bool polarity = false; //tells us whether we're drawing odds or evens
	osThreadSuspend(drawHandle); //suspend right at startup

	while(true) {
		//if we have a new message from the queue, update the draw buffer
		if(osMessageQueueGetCount(drawbuf_queue) > 0) {
			osStatus_t status; //storing status for debugging purposes
			status = osMessageQueueGet(drawbuf_queue, &draw_buffer, NULL, 0);
			UNUSED(status);
		}

		if(polarity) bargraph_output(draw_buffer & 0x155); //turn on the corresponding ODD leds according to the display buffer
		else bargraph_output(draw_buffer & 0x2AA); //turn on the corresponding EVEN leds according to the display buffer
		polarity = !polarity;

		//delay for the next screen update
		osDelay(BARGRAPH_UPDATE_DELAY);
	}

	//should never reach here but exiting gracefully
	osThreadExit();
}
