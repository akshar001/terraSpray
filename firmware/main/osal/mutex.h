#ifndef OSAL_MUTEX_H
#define OSAL_MUTEX_H

#include <stdint.h>
#include <stdbool.h>

struct osal_mutex;

/**
 * @brief Create a mutex.
 *
 * @return A handle of mutex.
 */
extern struct osal_mutex* osal_mutex_create();

/**
 * @brief Obtain a mutex.
 */
extern void osal_mutex_lock(struct osal_mutex* mutex);

/**
 * @brief Obtain a mutex with timeout.
 * @return false if the mutex was obtained. true if timeout.
 */
extern bool osal_mutex_lock_timeout(struct osal_mutex* mutex, uint32_t timeout);

/**
 * @brief Release a mutex.
 */
extern void osal_mutex_unlock(struct osal_mutex* mutex);

#endif /* OSAL_MUTEX_H */
