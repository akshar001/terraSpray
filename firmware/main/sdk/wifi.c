#include <string.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "osal/memory.h"
#include "osal/times.h"
#include "network.h"
#include "cJSON.h"
#include "wifi.h"
#include "config.h"

#define TAG "WIFI"

/**< Define a static variable to update status of access point */
static bool wifi_flag_connection;

void wifi_init(void)
{
  wifi_flag_connection = false;
  hal_net_init();
  wifi_connect();
}

bool wifi_is_connected()
{
  return wifi_flag_connection;
}

esp_err_t wifi_connect()
{
  bool status = true;
  cfg_data ssid;
  cfg_data password;
  esp_err_t error = ESP_OK;
  memset(ssid.str, '\0', CFG_STR_LEN);
  memset(password.str, '\0', CFG_STR_LEN);

  error = config_get_data(CFG_NETWORK_SSID, CFG_STR, &ssid);

  if (error != ESP_OK)
  {
    ESP_LOGE(TAG, "Error: Failed to get ssid");
    return ESP_FAIL;
  }

  error = config_get_data(CFG_NETWORK_PASS, CFG_STR, &password);
  if (error != ESP_OK)
  {
    ESP_LOGE(TAG, "Error: Failed to get password");
    return ESP_FAIL;
  }
  ssid.str[strlen(ssid.str)] = '\0';
  password.str[strlen(password.str)] = '\0';
  ESP_LOGI(TAG, "Create access point %s", ssid.str);

  if (hal_net_set_ap_config(ssid.str, password.str, HAL_NET_ENC_WPA2_PSK) != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to set access point mode");
    return ESP_FAIL;
  }

  if (hal_net_start_ap() != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to start access point mode");
    return ESP_FAIL;
  }

  osal_time_delay(1);

  wifi_flag_connection = true;

  return status;
}

void wifi_disconnect()
{
  if (hal_net_stop_ap() != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to stop access point mode");
    return;
  }
}
