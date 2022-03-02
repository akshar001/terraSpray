#include <string.h>
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "hal/system.h"
#include "osal/memory.h"


#define LOG_TAG "SYSTEM"

/**
 * @brief Reboot device.
 */
void hal_system_reboot(void)
{
    esp_restart();
}

void hal_system_deep_sleep(uint64_t milisecond)
{
    esp_deep_sleep(milisecond * 1000);
}

enum hal_system_wakeup_reason hal_system_get_wakeup_reason(void)
{
    return esp_sleep_get_wakeup_cause();
}

esp_err_t drv_sleep_enable_ext1_wakeup(uint64_t gpios, esp_sleep_ext1_wakeup_mode_t mode)
{
    return esp_sleep_enable_ext1_wakeup(gpios, mode);
}

