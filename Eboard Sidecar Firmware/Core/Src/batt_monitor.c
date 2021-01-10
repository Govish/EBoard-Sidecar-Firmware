#include "batt_monitor.h"

//======================= some defines ======================
#define DMA_COMPLETE_FLAG (1<<0) //flag that will be asserted when the DMA ADC read is complete
#define ADC_READY_FLAG (1<<1) //ADC mutex, effectively
#define SOC_LOW_FLAG (1<<2) //flag asserted when SOC is "low"
#define SOC_CRIT_FLAG (1<<3) //flag asserted whe SOC is "critical"
#define SOC_MEASURE_FAIL (1<<4) //flag asserted when the monitor thread fails to read the ADC multiple times

#define ADC_OVERSAMPLES 16
#define SAMPLE_BUFFER_LEN 256
#define ADC_READ_TIMEOUT 100 //can take 100 ticks max to read the ADC before throwing a timeout error
#define ADC_MAX_READ_FAILS 8 //how many times the ADC read can fail before asserting the SOC_MEASURE_FAIL flag
#define DIVIDER_RATIO 0.00887937 //adc bits to volts

#define MONITOR_UPDATE_DELAY 10 //how long the monitor loop should wait for

//===================== PRIVATE VARIABLES =====================
static osEventFlagsId_t monitor_util_flags; //way for the ISR to signal to the main thread
static osMessageQueueId_t soc_buf; //a queue that we'll put the updated SOC values into

static osThreadId_t monitor_handle = NULL; //handle for the SOC monitoring thread

//==================== PRIVATE FUNCTION PROTOTYPES ===================
static void run_monitor(void* argument); //thread function for SOC monitor

//perform an ADC DMA read
//return type is os status e.g. osOK
static uint32_t do_adc_dma(ADC_HandleTypeDef *hadc, uint16_t *buffer, uint32_t len, uint32_t timeout);

// ================== PUBLIC FUNCTION DEFS ==================
osMessageQueueId_t monitor_init() {
	monitor_util_flags = osEventFlagsNew(NULL); //create the monitor signaling flag
	soc_buf = osMessageQueueNew(1, sizeof(float), NULL); //creating the SOC buffer/queue

	osEventFlagsSet(monitor_util_flags, ADC_READY_FLAG); //adc is now ready since everything is initialized

	return soc_buf;
}

//initialize and start the monitor thread function
void monitor_start(ADC_HandleTypeDef *hadc) {
	const osThreadAttr_t monitor_attributes = {
			.name = "monitor",
			.priority = (osPriority_t) osPriorityAboveNormal,
			.stack_size = 2048
	};
	monitor_handle = osThreadNew(run_monitor, (void*)hadc, &monitor_attributes);
}

bool v_sys_check(float min_voltage, ADC_HandleTypeDef *hadc) {
	uint16_t adc_results[ADC_OVERSAMPLES];
	uint32_t adc_status;

	//floor the min_voltage to SANE_VOLTAGE_LOWER_LIMIT
	min_voltage = min_voltage < SANE_VOLTAGE_LOWER_LIMIT ? SANE_VOLTAGE_LOWER_LIMIT : min_voltage;

	//read the ADC via DMA
	adc_status = do_adc_dma(hadc, adc_results, ADC_OVERSAMPLES, ADC_READ_TIMEOUT);
	if(adc_status & (1<<31)) return false; //if we timed out or something weird happened, fail the check

	//compute the system voltage from the average of the ADC readings
	uint32_t adc_sum = 0;
	float v_sys = 0;
	for(int i = 0; i < ADC_OVERSAMPLES; i++) {
		adc_sum += adc_results[i];
	}
	v_sys = adc_sum * DIVIDER_RATIO / ADC_OVERSAMPLES;

	//if the measured system voltage is sane
	return (v_sys > min_voltage) && (v_sys < SANE_VOLTAGE_UPPER_LIMIT);
}

bool monitor_soc_low(bool clear_flag) {
	bool result = osEventFlagsGet(monitor_util_flags) & SOC_LOW_FLAG;
	if(result && clear_flag) osEventFlagsClear(monitor_util_flags, SOC_LOW_FLAG);
	return result;
}

bool monitor_soc_crit(bool clear_flag) {
	bool result = osEventFlagsGet(monitor_util_flags) & SOC_CRIT_FLAG;
	if(result && clear_flag) osEventFlagsClear(monitor_util_flags, SOC_CRIT_FLAG);
	return result;
}

bool monitor_read_fail(bool clear_flag) {
	bool result = osEventFlagsGet(monitor_util_flags) & SOC_MEASURE_FAIL;
	if(result && clear_flag) osEventFlagsClear(monitor_util_flags, SOC_MEASURE_FAIL);
	return result;
}

//return the free stack space of the monitor thread
uint32_t monitor_stack_space() {return osThreadGetStackSpace(monitor_handle);}

// ==================== PRIVATE FUNCTION DEFINITIONS =====================
static void run_monitor(void* argument) {
	ADC_HandleTypeDef *hadc = (ADC_HandleTypeDef*) argument;
	float sample_buffer[SAMPLE_BUFFER_LEN]; //buffer to compute the moving average voltage reading
	float mav_voltage = 0, soc = 0; //moving average of system voltage measurement
	uint16_t buffer_pointer = 0; //for our circular buffer
	uint8_t read_fail_counter = 0;
	bool soc_low_asserted = false;

	//initialize all values of the sample buffer array
	for(int i = 0; i < SAMPLE_BUFFER_LEN; i++) {
		sample_buffer[i] = MAV_INIT_VALUE;
	}

	while(1) {
		//create a status and other local variables
		uint32_t adc_status;
		uint16_t adc_results[ADC_OVERSAMPLES];
		float adc_voltage;

		//attempt to read the ADC
		adc_status = do_adc_dma(hadc, adc_results, ADC_OVERSAMPLES, ADC_READ_TIMEOUT);

		//compute the ADC voltage (will be invalid if the adc timed out, but whatev, we'll handle that)
		uint32_t adc_sum = 0;
		for(int i = 0; i < ADC_OVERSAMPLES; i++) {
			adc_sum += adc_results[i];
		}
		adc_voltage = adc_sum * DIVIDER_RATIO / ADC_OVERSAMPLES;

		//if the ADC read was successful and the voltage is sane
		if(((adc_status & (1<<31)) == 0) && (adc_voltage < SANE_VOLTAGE_UPPER_LIMIT) && (adc_voltage > SANE_VOLTAGE_LOWER_LIMIT)) {

			//divide the ADC voltage by SAMPLE_BUFFER_LENGTH and store it at the current pointer location
			float scaled_voltage = adc_voltage / (float)SAMPLE_BUFFER_LEN;
			sample_buffer[buffer_pointer] = scaled_voltage;

			//sum up the entire contents of the sample buffer
			mav_voltage = 0;
			for(int i = 0; i < SAMPLE_BUFFER_LEN; i++) {
				mav_voltage += sample_buffer[i];
			}

			//compute the SOC from 0 to 1 and store that in the soc queue
			soc = (mav_voltage - MIN_VOLTAGE)/(MAX_VOLTAGE - MIN_VOLTAGE);
			osMessageQueueReset(soc_buf);
			osMessageQueuePut(soc_buf, &soc, 0, 0);


			//check if that sum meets the thresholds for low and critical levels (and assert those flags if appropriate)
			if(mav_voltage < SOC_VOLTAGE_CRITICAL) {
				osEventFlagsSet(monitor_util_flags, SOC_CRIT_FLAG);
			}
			else if(mav_voltage < SOC_VOLTAGE_LOW && !soc_low_asserted) {
				osEventFlagsSet(monitor_util_flags, SOC_LOW_FLAG);
				soc_low_asserted = true; //latch this so we only trigger once
			}

			//increment/wrap around the buffer pointer
			buffer_pointer = (buffer_pointer >= (SAMPLE_BUFFER_LEN-1)) ? 0 : buffer_pointer+1;

			//reset the read fail counter
			read_fail_counter = 0;
		}
		//if the ADC read was unsuccessful or the voltage is insane
		else {
			//increment the read fail counter
			read_fail_counter++;

			//if the read fail counter exceeds the fail threshold, assert the appropriate flag
			if(read_fail_counter >= ADC_MAX_READ_FAILS) osEventFlagsSet(monitor_util_flags, SOC_MEASURE_FAIL);
		}

		osDelay(MONITOR_UPDATE_DELAY);
	}

	osThreadExit(); //exit gracefully if the function somehow gets here?
}

static uint32_t do_adc_dma(ADC_HandleTypeDef *hadc, uint16_t *buffer, uint32_t len, uint32_t timeout) {
	uint32_t start_tick = HAL_GetTick();
	uint32_t status; //see if our event flag wait timed out or not
	uint32_t dma_timeout; //computed after the resource is released

	//wait for the ADC to be free, then lock it out (subject to a timeout)
	status = osEventFlagsWait(monitor_util_flags, ADC_READY_FLAG, osFlagsWaitAny, timeout);
	if(status & (1<<31)) return status;
	dma_timeout = (timeout + start_tick) - HAL_GetTick(); //how much remaining time the DMA read should take to meet the timeout

	//read adc via DMA and wait till it's done
	osEventFlagsClear(monitor_util_flags, DMA_COMPLETE_FLAG); //clear the DMA complete flag before starting the next DMA request (just to make sure)
	HAL_ADC_Stop_DMA(hadc); //clear the appropriate DMA bits
	HAL_ADC_Start_DMA(hadc, (uint32_t*)buffer, len); //restart the DMA request
	status = osEventFlagsWait(monitor_util_flags, DMA_COMPLETE_FLAG, osFlagsWaitAny, dma_timeout); //wait for DMA to be complete

	//release the ADC resource
	osEventFlagsSet(monitor_util_flags, ADC_READY_FLAG);
	return status; //return whether the DMA completed successfully or whether it timed out
}

// ======================== ISRs =========================

//service the interrupt that gets called when the ADC DMA request completes
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	//just set the DMA completion event flag here
	osEventFlagsSet(monitor_util_flags, DMA_COMPLETE_FLAG);
}
