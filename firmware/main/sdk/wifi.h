#ifndef SDK_WIFI_H
#define SDK_WIFI_H

#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "cJSON.h"

/**
 * @brief Initialize WIFI module
 *
 * @return None.
 */
void wifi_init(void);

/**
 * @brief Establish access point network from environment
 *
 * @return The error code.
 */
esp_err_t wifi_connect(void);

/**
 * @brief Disconnect WIFI
 *
 * @return None.
 */
void wifi_disconnect();

/**
 * @brief Get current status of WIFI to know it connected or not
 *
 * @return Status of API (true/false)
 */
bool wifi_is_connected();

#endif /* SDK_WIFI_H */
