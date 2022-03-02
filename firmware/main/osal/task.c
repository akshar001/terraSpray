#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "osal/memory.h"
#include "osal/task.h"

#include "times.h"


struct osal_task {
    TaskHandle_t handle;
    osal_task_coroutine coroutine;
    void* parameters;
    int error;
};

struct osal_timer {
    TimerHandle_t handle;
    osal_timer_coroutine coroutine;
    int error;
};
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
osal_task_handle osal_task_create(osal_task_coroutine coroutine, const char* const name,
        const uint32_t stack_size, void* const parameters, uint32_t priority)
{
    struct osal_task* task;

    task = (struct osal_task*)osal_mem_malloc(sizeof(struct osal_task));
    task->coroutine = coroutine;
    task->parameters = parameters;

    void root_coroutine(void* parameters)
    {
        struct osal_task* task = parameters;

        task->coroutine(task->parameters);

        osal_mem_free(task);
        osal_task_exit();
    }
    task->error = xTaskCreate(root_coroutine, name, stack_size, task, priority, &task->handle);
    return task;
}

/**
 * @brief Set the priority.
 *
 * @param task A handle of task
 * @param priority The priority at which the task should run
 * @return A handle of task.
 */
osal_task_handle osal_task_set_priority(osal_task_handle task, uint32_t priority)
{
    struct osal_task* _task;
    _task = (struct osal_task*)task;

    vTaskPrioritySet(_task->handle, priority);
    _task->error = pdTRUE;
    return task;
}

/**
 * @brief Get the error of previous operator.
 *
 * @param task A handle of task
 * @return The error code.
 */
int osal_task_get_error(osal_task_handle task)
{
    struct osal_task* _task;
    _task = (struct osal_task*)task;

    return _task->error;
}

/**
 * @brief Delete a task.
 *
 * @param task A handle of task
 */
void osal_task_delete(osal_task_handle task)
{
    struct osal_task* _task;
    _task = (struct osal_task*)task;

    vTaskDelete(_task->handle);
    osal_mem_free(task);
}

/**
 * @brief Exit a task.
 *
 * @param task A handle of task
 */
void osal_task_exit()
{
    vTaskDelete(NULL);
}

/**
 * @brief Start main looper.
 */
void osal_task_start_looper()
{
    while (true) {
        osal_time_sleep(OSAL_TASK_MAIN_LOOP_SLEEP_INTERVAL);
    }
}

osal_task_handle osal_task_create_timer(osal_timer_coroutine coroutine, const char* const name, const uint32_t period, bool isReload)
{
  struct osal_timer* task;

  task = (struct osal_timer*)osal_mem_malloc(sizeof(struct osal_timer));
  task->coroutine = coroutine;
  task->handle = xTimerCreate(name, period, isReload ? pdTRUE : pdFALSE, (void*)0, task->coroutine);
  return task;
}

void osal_task_start_timer(osal_task_handle task)
{
  struct osal_timer* _task = (struct osal_timer*)task;
  xTimerStart(_task->handle, 0);
}

void osal_task_stop_timer(osal_task_handle task)
{
  struct osal_timer* _task = (struct osal_timer*)task;
  xTimerStop(_task->handle, 0);
  osal_mem_free(task);
}

/* 
 * @brief: Send notify signal from ISR
 */
void osal_task_notify_from_isr(osal_task_handle task, uint32_t ulValue)
{
    struct osal_task* _task = (struct osal_task*)task;

    xTaskNotifyFromISR(_task->handle, ulValue, eSetBits, NULL);
    portYIELD_FROM_ISR();
}

/* 
 * @brief: Send notify signal
 */
void osal_task_notify(osal_task_handle task, uint32_t ulValue)
{
    struct osal_task* _task = (struct osal_task*)task;

    xTaskNotify(_task->handle, ulValue, eSetBits);
}

static bool osal_task_notify_wait_internal(uint32_t* notification_value, const TickType_t timeout)
{
    bool ret = false;

    /* Wait to be notified of an interrupt. */
    BaseType_t result = xTaskNotifyWait(pdFALSE,    /* Don't clear bits on entry. */
        0xFFFFFFFF,        /* Clear all bits on exit. */
        notification_value, /* Stores the notified value. */
        timeout);
    if (result == pdPASS) ret = true;

    return ret;
}

bool osal_task_notify_wait(uint32_t* notification_value)
{
    return osal_task_notify_wait_internal(notification_value, portMAX_DELAY);
}

bool osal_task_notify_wait_timeout(uint32_t* notification_value, uint32_t timeout)
{
    return osal_task_notify_wait_internal(notification_value, osal_time_ms_to_tick(timeout));
}
