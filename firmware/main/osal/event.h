#ifndef OSAL_EVENT_H
#define OSAL_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

struct osal_event {
    EventGroupHandle_t event;
};

typedef uint32_t osal_event_signal;
/**
 * @brief Create a event.
 *
 * @return A handle of event.
 */
extern struct osal_event* osal_event_create();

/**
 * @brief Delete a event.
 *
 */
extern void osal_event_delete(struct osal_event* event);

/**
 * @brief Wait for an event.
 */
extern osal_event_signal osal_event_wait(struct osal_event* event, osal_event_signal event_signal_mask);

/**
 * @brief Wait for an event with timeout.
 * @return false if the mutex was triggered. true if timeout.
 */
extern osal_event_signal osal_event_wait_timeout(struct osal_event* event, osal_event_signal event_signal_mask, uint32_t timeout);

/**
 * @brief Trigger an event.
 */
extern void osal_event_trigger(struct osal_event* event, osal_event_signal signal);

/**
 * @brief Reset an event.
 */
extern void osal_event_reset(struct osal_event* event, osal_event_signal signal);

#endif /* OSAL_EVENT_H */
