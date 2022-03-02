#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp32/rom/ets_sys.h"
#include "times.h"


/**
 * @brief Sleep for seconds.
 *
 * This API will suspend current task.
 *
 * @param seconds How many seconds you want to sleep
 */
void osal_time_sleep(uint32_t seconds)
{
    osal_time_sleep_ms(seconds * 1000);
}

/**
 * @brief Sleep for miliseconds.
 *
 * This API will suspend current task.
 *
 * @param miliseconds How many miliseconds you want to sleep
 */
void osal_time_sleep_ms(uint32_t miliseconds)
{
    vTaskDelay(osal_time_ms_to_tick(miliseconds));
}

/**
 * @brief Busy wait for seconds.
 *
 * @param seconds How many seconds you want to delay
 */
void osal_time_delay(uint32_t seconds)
{
    osal_time_delay_us(seconds * 1000000);
}

/**
 * @brief Busy wait for miliseconds.
 *
 * @param miliseconds How many miliseconds you want to wait
 */
void osal_time_delay_ms(uint32_t miliseconds)
{
    osal_time_delay_us(miliseconds * 1000);
}

/**
 * @brief Busy wait for microseconds.
 *
 * @param microseconds How many microseconds you want to wait
 */
void osal_time_delay_us(uint32_t microseconds)
{
    ets_delay_us(microseconds);
}

/**
 * @brief Get boot time.
 *
 * @return The boot time in millisecond.
 */
uint32_t osal_time_get_boot_time()
{
    return osal_time_tick_to_ms(xTaskGetTickCount());
}
