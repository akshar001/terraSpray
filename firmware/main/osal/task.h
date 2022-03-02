#ifndef OSAL_TASK_H
#define OSAL_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/timers.h"

#define OSAL_TASK_MAIN_LOOP_SLEEP_INTERVAL 3600

typedef void* osal_task_handle;

/**
 * @brief The type of task coroutine.
 */
typedef void (*osal_task_coroutine)(void*);

/**
 * @brief The type of timer coroutine.
 */
typedef void (*osal_timer_coroutine)(void*);

/**
 * @brief Create a task.
 *
 * @param coroutine A pointer to the task entry point
 * @param name A descriptive name for the task
 * @param stack_size The size of the task stack specified as the number of bytes
 * @param user_data A user defined data
 * @param priority The priority at which the task should run
 * @return A handle of task.
 */
extern osal_task_handle osal_task_create(osal_task_coroutine coroutine, const char* const name,
        const uint32_t stack_size, void* const user_data, uint32_t priority);

/**
 * @brief Set the priority.
 *
 * @param task A handle of task
 * @param priority The priority at which the task should run
 * @return A handle of task.
 */
extern void* osal_task_set_priority(osal_task_handle task, uint32_t priority);

/**
 * @brief Get the error of previous operator.
 *
 * @param task A handle of task
 * @return The error code.
 */
extern int osal_task_get_error(osal_task_handle task);

/**
 * @brief Delete a task.
 *
 * @param task A handle of task
 */
extern void osal_task_delete(osal_task_handle task);

/**
 * @brief Exit a task.
 *
 * @param task A handle of task
 */
void osal_task_exit();

/**
 * @brief Start main looper.
 */
extern void osal_task_start_looper();

extern osal_task_handle osal_task_create_timer(osal_timer_coroutine coroutine, const char* const name, const uint32_t period, bool isReload);
extern void osal_task_start_timer(osal_task_handle task);
extern void osal_task_stop_timer(osal_task_handle task);
extern void osal_task_notify_from_isr(osal_task_handle task, uint32_t ulValue);
extern void osal_task_notify(osal_task_handle task, uint32_t ulValue);
extern bool osal_task_notify_wait(uint32_t* notification_value);
extern bool osal_task_notify_wait_timeout(uint32_t* notification_value, uint32_t timeout);
#endif /* OSAL_TASK_H */
