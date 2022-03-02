#ifndef OSAL_TIME_H
#define OSAL_TIME_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"

#define osal_time_ms_to_tick(miliseconds) (miliseconds / portTICK_PERIOD_MS)
#define osal_time_tick_to_ms(tick) (tick * portTICK_PERIOD_MS)

/**
 * @brief Sleep for seconds.
 *
 * This API will suspend current task.
 *
 * @param seconds How many seconds you want to sleep
 */
extern void osal_time_sleep(uint32_t seconds);

/**
 * @brief Sleep for miliseconds.
 *
 * This API will suspend current task.
 *
 * @param miliseconds How many miliseconds you want to sleep
 */
extern void osal_time_sleep_ms(uint32_t miliseconds);

/**
 * @brief Busy wait for seconds.
 *
 * @param seconds How many seconds you want to delay
 */
extern void osal_time_delay(uint32_t seconds);

/**
 * @brief Busy wait for miliseconds.
 *
 * @param miliseconds How many miliseconds you want to wait
 */
extern void osal_time_delay_ms(uint32_t miliseconds);

/**
 * @brief Busy wait for microseconds.
 *
 * @param microseconds How many microseconds you want to wait
 */
extern void osal_time_delay_us(uint32_t microseconds);

/**
 * @brief Get boot time.
 *
 * @return The boot time in millisecond.
 */
extern uint32_t osal_time_get_boot_time();

#endif /* OSAL_TIME_H */
