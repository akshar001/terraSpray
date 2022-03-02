#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "osal/memory.h"
#include "osal/mutex.h"

#include "times.h"

struct osal_mutex {
    SemaphoreHandle_t lock;
};

struct osal_mutex* osal_mutex_create()
{
    struct osal_mutex* mutex = osal_mem_malloc(sizeof(struct osal_mutex));
    mutex->lock = xSemaphoreCreateMutex();
    return mutex;
}

static bool osal_mutex_lock_internal(struct osal_mutex* mutex, const TickType_t timeout)
{
    return xSemaphoreTake(mutex->lock, timeout) == pdTRUE ? false : true;
}

void osal_mutex_lock(struct osal_mutex* mutex)
{
    assert(osal_mutex_lock_internal(mutex, portMAX_DELAY) == false);
}

bool osal_mutex_lock_timeout(struct osal_mutex* mutex, uint32_t timeout)
{
    return osal_mutex_lock_internal(mutex, osal_time_ms_to_tick(timeout));
}

void osal_mutex_unlock(struct osal_mutex* mutex)
{
    xSemaphoreGive(mutex->lock);
}
