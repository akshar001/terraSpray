#ifndef MAIN_SDK_APP_H_
#define MAIN_SDK_APP_H_

#include <stdint.h>
#include <stdbool.h>

#define APP_NUMBER_OF_DAY   (7)

/**< App will support 5 time-set in one-day */
#define APP_SUPPORT_SIZE    (5)

//PIN define
#define LED_GPIO 5
#define BUTTON_WAKEUP_GPIO 21

/**<
 * Brief:  Define scheduler structure to store configuration time
 *
 * Description: The solution to handle input hh:mm <start time> and
 * hh:mm <stop time> from User.
 *
 * Idea:
 * Q1: How to get 00:00:00 of a day ?
 * Answer: Timestamp - (Timestamp % (24 * 60 * 60)
 * Example 1593327506 -> Get 00:00:00 of that day ? -> 1593302400 -> 06/08/2020 @12:00AM (UTC)
 *
 * IDEA: Scheduler will follow offset of a day (Q1) + Convert hh:mm to s of a day. Maximum (60 * 60 * 24 = 86400) ~ uint32_t
 * IDEA: Interval time will calculate with minutes unit -> Maximum (24 * 60 * 60 = 86400) ~ uint32_t
 **/
typedef struct {
	uint32_t start_time;
	uint32_t stop_time;
	uint32_t interval_time;
	uint8_t	index;
	bool active;
} scheduler_t;

/**
 * num: number of active
 */
typedef struct {
	scheduler_t scheduler[APP_SUPPORT_SIZE];
	uint8_t num;
} day_scheduler_t;

typedef struct {
	day_scheduler_t day[APP_NUMBER_OF_DAY];
} app_scheduler_t;

typedef enum {
	DEEP_SLEEP_BOTH = 0x00,
	DEEP_SLEEP_BUTTON = 0x01,
	DEEP_SLEEP_TIMER = 0x02,
} DEEP_SLEEP_CAUSE_E;

typedef struct {
	scheduler_t sleep_scheduler;
	uint32_t last_start_time;
	uint32_t sleep_time;
	uint32_t remain_time;
	bool isrunning;
	bool lastround;
	bool isready;
} sleep_schedule_t;

/**
 * Brief: Initialize scheduler with dummy value ALL ZERO
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_init(void);

/**
 * Brief: Set scheduler based on input from User
 *
 * param[in]: day of week. Example 0 is Sunday, 1 is Monday etc... 5 is Friday.
 * param[in]: scheduler input scheduler with start/stop time and interval time. All of input is seconds
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_set_scheduler(int day, scheduler_t scheduler);

/**
 * Brief: Get scheduler based on input from User
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_get_scheduler(void);

/**
 * Brief: Get scheduler based on time of each scheduler then return the most fit scheduler base on time
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: scheduler_t the most fit scheduler
 */
scheduler_t app_get_scheduler_base_on_time(void);

/**
 * Brief: Check input scheduler base on current day's scheduler and resort the day's scheduler with new input
 *
 * param[in]: scheduler input from user
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_check_input_scheduler(scheduler_t scheduler);

/**
 * Brief: Arrange the scheduler list
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_arragne_scheduler_list(void);

/**
 * Brief: Remove select scheduler out of the list
 *
 * param[in]: postion of the remove scheduler
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_remove_scheduler(uint8_t day_r, uint8_t position);

void app_remove_scheduler_pass(uint8_t index_del);

void app_set_all_scheduler(app_scheduler_t app_scheduler);
app_scheduler_t app_get_all_scheduler(void);

/**
 * Brief: Initalize deep sleep
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_init_deep_sleep(DEEP_SLEEP_CAUSE_E cause, uint32_t sleeptime);

void app_job(void);
/**
 * Brief: Initalize scheduler
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: None
 */
void app_init_scheduler(void);

/**
 * Brief: Get current time base on 00:00:00 and return
 *
 * param[in]: None
 * param[out]: None
 * param[in/out]: None
 * return: Current time
 */
uint32_t app_get_current_time(void);



void app_set_old_millis();

void store_gmt();

void retrive_gmt();

#endif /* MAIN_SDK_APP_H_ */
