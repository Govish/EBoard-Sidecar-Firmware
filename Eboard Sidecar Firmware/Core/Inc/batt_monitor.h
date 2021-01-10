#ifndef BATT_MONITOR_H
#define BATT_MONITOR_H

#include "stm32f4xx_hal.h"
#include "stdbool.h"
#include "cmsis_os.h"

#define MIN_VOLTAGE 24.0f
#define MAX_VOLTAGE 33.6f
#define SANE_VOLTAGE_UPPER_LIMIT 35.0f //any ADC reading above this should throw some sorta error
#define SANE_VOLTAGE_LOWER_LIMIT 10.0f //any ADC reading below this should throw some sorta error

#define SOC_VOLTAGE_LOW 26.0 //voltage that will trigger the SOC_LOW flag
#define SOC_VOLTAGE_CRITICAL 24.0 //voltage that will trigger the SOC_CRIT flag
#define MAV_INIT_VALUE 0.10546875f //initialization value for the moving average buffer (works out to 27V)

//called on application start; checks system voltage and returns true if good
bool v_sys_check(float min_voltage, ADC_HandleTypeDef *hadc);

//initializes some of the os-related aspects of the monitor
//returns a pointer to the SOC queue that will be updated by the monitor thread
osMessageQueueId_t monitor_init();

//start the actual monitoring thread
void monitor_start(ADC_HandleTypeDef *hadc);

//=========== some functions to make reading/clearing monitoring flags easy ===========
bool monitor_soc_low(bool clear_flag);
bool monitor_soc_crit(bool clear_flag);
bool monitor_read_fail(bool clear_flag);
//=================================================================================

uint32_t monitor_stack_space();

#endif /* INC_BAT_MONITOR_H_ */
