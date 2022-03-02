#include "osal/memory.h"
#include "osal/event.h"

#include "times.h"

struct osal_event* osal_event_create()
{
    struct osal_event* event = osal_mem_malloc(sizeof(struct osal_event));
    event->event = xEventGroupCreate();
    return event;
}

void osal_event_delete(struct osal_event* event)
{
    if (event == NULL) return;

    vEventGroupDelete(event->event);
    osal_mem_free(event);
}

bool osal_event_wait_internal(struct osal_event* event, const TickType_t timeout)
{
    TickType_t begin = xTaskGetTickCount();
    TickType_t now = xTaskGetTickCount();

    uint32_t bits = 0x00;
    while ((now - begin) < timeout) {
        TickType_t remaining_timeout = timeout - (now - begin);
        bits = xEventGroupWaitBits(event->event, 0x01, pdTRUE, pdFALSE, remaining_timeout);
        if (bits == 0x01) break;
        now = xTaskGetTickCount();
    }
    return bits != 0x01;
}

osal_event_signal osal_event_wait(struct osal_event* event, osal_event_signal event_signal_mask)
{
  return xEventGroupWaitBits(event->event, event_signal_mask, pdTRUE, pdFALSE, portMAX_DELAY);
}

osal_event_signal osal_event_wait_timeout(struct osal_event* event, osal_event_signal event_signal_mask, uint32_t timeout)
{
  return xEventGroupWaitBits(event->event, event_signal_mask, pdTRUE, pdFALSE, timeout);
}

void osal_event_trigger(struct osal_event* event, osal_event_signal signal)
{
    xEventGroupSetBits(event->event, signal);
}

void osal_event_reset(struct osal_event* event, osal_event_signal signal)
{
    xEventGroupClearBits(event->event, signal);
}
