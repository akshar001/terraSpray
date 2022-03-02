#ifndef HAL_SYSTEM_H
#define HAL_SYSTEM_H

#include "esp_sleep.h"
#include "driver/gpio.h"

enum hal_system_wakeup_reason {
    HAL_SYSTEM_WAKEUP_NONE = ESP_SLEEP_WAKEUP_UNDEFINED,   //!< In case of deep sleep, reset was not caused by exit from deep sleep
    HAL_SYSTEM_WAKEUP_EXT0 = ESP_SLEEP_WAKEUP_EXT0,   //!< Wakeup caused by external signal using RTC_IO
    HAL_SYSTEM_WAKEUP_EXT1 = ESP_SLEEP_WAKEUP_EXT1,   //!< Wakeup caused by external signal using RTC_CNTL
    HAL_SYSTEM_WAKEUP_TIMER = ESP_SLEEP_WAKEUP_TIMER, //!< Wakeup caused by timer
    HAL_SYSTEM_WAKEUP_ULP = ESP_SLEEP_WAKEUP_ULP     //!< Wakeup caused by ULP program
};

/**
 * @brief Reboot device.
 */
extern void hal_system_reboot(void);

extern void hal_system_deep_sleep(uint64_t milisecond);

extern void hal_system_enable_ext1_wakeup(uint64_t gpios, esp_sleep_ext1_wakeup_mode_t mode);

extern enum hal_system_wakeup_reason hal_system_get_wakeup_reason(void);

#endif /* HAL_SYSTEM_H */
