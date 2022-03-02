#include <string.h>
#include <stdbool.h>
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "ping/ping.h"
#include "esp_ping.h"

#include "network.h"
#include "memory.h"
#include "time.h"
#include "esp_log.h"

#define TAG "NETWORK"
#define NETWORK_SIGNAL_EVENT_CONNECT (0x01)

struct hal_net {
  osal_task_handle reconnect_task;
  osal_task_handle scan_task;
  int reconnect_count;
  int prev_reconnect_delay;
  int reconnect_delay;
  struct osal_event* reconnect_cancel;
  struct osal_mutex* reconnect_lock;
  enum hal_net_sta_state sta_state;
  enum hal_net_sta_connect_error sta_connect_error;
  hal_net_on_sta_state_change on_sta_state_change_listener;
  void* on_sta_state_change_user_data;
  hal_net_on_ping_result on_ping_result;
};

static struct hal_net* net = NULL;

static void hal_net_reconnect();

static const char* hal_net_get_encryption_name(int encryption)
{
  const char* ret;
  switch (encryption) {
    case WIFI_AUTH_OPEN:
      ret = HAL_NET_ENC_NONE;
      break;
    case WIFI_AUTH_WEP:
      ret = HAL_NET_ENC_WEP;
      break;
    case WIFI_AUTH_WPA_PSK:
      ret = HAL_NET_ENC_WPA_PSK;
      break;
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
      ret = HAL_NET_ENC_WPA2_PSK;
      break;
    default:
      ret = HAL_NET_ENC_NONE;
  }
  return ret;
}

static void scan_done_handler(void)
{
  uint16_t sta_number = 0;
  esp_wifi_scan_get_ap_num(&sta_number);

  struct hal_net_ap_data* aps = osal_mem_malloc(sta_number * sizeof(struct hal_net_ap_data));
  wifi_ap_record_t* esp_aps = osal_mem_malloc(sta_number * sizeof(wifi_ap_record_t));

  esp_err_t err = esp_wifi_scan_get_ap_records(&sta_number, esp_aps);
  if (err == ESP_OK) {
    for(int i = 0; i < sta_number; i++) {
      memcpy(aps[i].ssid, esp_aps[i].ssid, 32);
      aps[i].ssid[32] = '\x0';
      aps[i].rssi = esp_aps[i].rssi;
      aps[i].encryption = hal_net_get_encryption_name(esp_aps[i].authmode);
    }
  }
  free(esp_aps);
}

static void set_sta_connect_error(enum hal_net_sta_connect_error err)
{
  net->sta_connect_error = err;
}

static void set_sta_state(enum hal_net_sta_state state)
{
  if (net->sta_state == state) return;

  enum hal_net_sta_state old = net->sta_state;
  net->sta_state = state;
  net->on_sta_state_change_listener(old, state, net->on_sta_state_change_user_data);
}

static void on_disconnect_handler(void *ctx, system_event_t *event)
{
  if ((net->sta_state != HAL_NET_STA_STATE_CONNECTED_WITHOUT_IP) ||
      (net->sta_state != HAL_NET_STA_STATE_CONNECTED)) {
    system_event_sta_disconnected_t *disconnected = &event->event_info.disconnected;
    if (disconnected->reason == WIFI_REASON_NO_AP_FOUND) {
      set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_AP_NOT_FOUND);
    } else {
      set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_INVALID_PASSWORD);
    }
  }

  esp_wifi_disconnect();
}

static void hal_net_connect()
{
  ESP_LOGI(TAG, "hal_net_connect");
  esp_err_t err = esp_wifi_connect();
  if (err == ESP_OK) return;
  ESP_LOGI(TAG, "failed hal_net_connect");
  set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_INVALID_PASSWORD);
  set_sta_state(HAL_NET_STA_STATE_RECONNECTING);
  if (net->reconnect_task == NULL) {
    ESP_LOGI(TAG, "Create hal_net_reconnect thread %d", net->reconnect_count);
    net->reconnect_task = osal_task_create(hal_net_reconnect, "hal_net_reconnect", 4096, NULL, 20);
  }
  osal_task_notify(net->reconnect_task, 0);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
      net->reconnect_count = 0;
      set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_NONE);
      set_sta_state(HAL_NET_STA_STATE_CONNECTING);
      hal_net_connect();
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
      set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_NONE);
      set_sta_state(HAL_NET_STA_STATE_CONNECTED);
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
      if (net->sta_state == HAL_NET_STA_STATE_CONNECTED) {
        set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_CANNOT_GET_IP);
        set_sta_state(HAL_NET_STA_STATE_CONNECTED_WITHOUT_IP);
      }
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      scan_done_handler();
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
      set_sta_connect_error(HAL_NET_STA_CONNECT_ERROR_CANNOT_GET_IP);
      set_sta_state(HAL_NET_STA_STATE_CONNECTED_WITHOUT_IP);
      if (net->reconnect_task != NULL) osal_task_notify(net->reconnect_task, 0);
      osal_event_trigger(net->reconnect_cancel, NETWORK_SIGNAL_EVENT_CONNECT);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
      on_disconnect_handler(ctx, event);
      set_sta_state(HAL_NET_STA_STATE_RECONNECTING);
      if (net->reconnect_task == NULL) {
        net->reconnect_task = osal_task_create(hal_net_reconnect, "hal_net_reconnect", 4096, NULL, 20);
      }
      osal_task_notify(net->reconnect_task, 0);
      break;
    default:
      break;
  }
  return ESP_OK;
}

static void hal_net_reconnect()
{
  ESP_LOGI(TAG, "Create hal_net_reconnect");
  osal_event_reset(net->reconnect_cancel, NETWORK_SIGNAL_EVENT_CONNECT);
  while (true) {
    uint32_t value;
    osal_task_notify_wait(&value);

    if (net->reconnect_count == 0) {
      net->prev_reconnect_delay = 1;
      net->reconnect_delay = 1;
    }

    uint32_t timeout_period = HAL_NET_MAX_RECONNECT_TIMEOUT < net->reconnect_delay ? HAL_NET_MAX_RECONNECT_TIMEOUT : net->reconnect_delay;
    osal_event_signal signal = osal_event_wait_timeout(net->reconnect_cancel, NETWORK_SIGNAL_EVENT_CONNECT, timeout_period * 1000);

    if (!(signal & NETWORK_SIGNAL_EVENT_CONNECT)) {
      ESP_LOGE(TAG, "Time out to reconnect");
      net->reconnect_count = 0;
      net->reconnect_task = NULL;
      break;
    }

    net->reconnect_delay = net->prev_reconnect_delay + net->reconnect_delay;
    net->prev_reconnect_delay = net->reconnect_delay - net->prev_reconnect_delay;

    ESP_LOGI(TAG, "hal_net_reconnect count: %d", net->reconnect_count);

    osal_mutex_lock_timeout(net->reconnect_lock, 10000);
    if ((net->sta_state == HAL_NET_STA_STATE_CONNECTED_WITHOUT_IP) ||
        (net->sta_state == HAL_NET_STA_STATE_CONNECTED)) {
      net->reconnect_count = 0;
      net->reconnect_task = NULL;
      osal_mutex_unlock(net->reconnect_lock);
      break;
    }
    else
    {
      net->reconnect_count = 0;
      net->reconnect_task = NULL;
      osal_mutex_unlock(net->reconnect_lock);
      break;
    }


    hal_net_connect();
    net->reconnect_count++;
  }
}

void on_sta_state_change_default(enum hal_net_sta_state old_state, enum hal_net_sta_state new_state, void* user_data)
{
  return;
}

/**
 * @brief Intialize hal network.
 *
 * This function must be called before you use any hal network APIs.
 */
hal_error hal_net_init(void)
{
  if (net == NULL) {
    net = osal_mem_malloc(sizeof(struct hal_net));
    net->reconnect_lock = osal_mutex_create();
    if (net->reconnect_lock == NULL)
    {
      ESP_LOGE(TAG, "Failed to create mutex for reconnect lock");
    }
    net->reconnect_count = 0;

    hal_net_set_on_sta_state_change_listener(NULL, NULL);
    set_sta_state(HAL_NET_STA_STATE_DISCONNECTED);

    tcpip_adapter_init();  
    
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
  
    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 1, 10);
    IP4_ADDR(&info.gw, 192, 168, 1, 10);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    ESP_LOGI(TAG,"setting gateway IP");
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));

    ESP_LOGI(TAG,"starting DHCPS adapter");
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    ESP_LOGI(TAG,"starting event loop");

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    ESP_LOGI(TAG,"initializing WiFi");
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

    net->reconnect_cancel = osal_event_create();
    net->reconnect_task = NULL;
    net->scan_task = NULL;
  }
  return HAL_RESULT_SUCCEED;
}

static hal_error hal_net_to_hal_error(esp_err_t esp_err)
{
  hal_error err;
  switch (esp_err) {
    case ESP_OK:
      err = HAL_RESULT_SUCCEED;
      break;
    case ESP_ERR_INVALID_ARG:
      err = HAL_ERR_INVALID_ARG;
      break;
    case ESP_ERR_WIFI_NOT_INIT:
      err = HAL_ERR_NET_WIFI_NOT_INIT;
      break;
    case ESP_ERR_WIFI_IF:
      err = HAL_ERR_NET_WIFI_INVALID_INTERFACE;
      break;
    case ESP_ERR_WIFI_MODE:
      err = HAL_ERR_NET_WIFI_INVALID_MODE;
      break;
    case ESP_ERR_WIFI_PASSWORD:
      err = HAL_ERR_NET_WIFI_INVALID_PASSWORD;
      break;
    case ESP_ERR_WIFI_NVS:
      err = HAL_ERR_NET_WIFI_INVALID_ENVS;
      break;
    case ESP_ERR_NO_MEM:
      err = HAL_ERR_NO_MEM;
      break;
    case ESP_ERR_WIFI_CONN:
      err = HAL_ERR_NET_WIFI_CONN;
      break;
    case ESP_ERR_WIFI_NOT_CONNECT:
      err = HAL_ERR_NET_WIFI_NOT_CONNECT;
      break;
    default:
      ESP_LOGE(TAG, "unknow esp error: %d", esp_err);
      err = HAL_ERR;
  }
  return err;
}

static int hal_net_get_encryption(const char* encryption)
{
  int ret = WIFI_AUTH_OPEN;
  if (strcmp(encryption, HAL_NET_ENC_NONE) == 0) {
    ret = WIFI_AUTH_OPEN;
  } else if (strcmp(encryption, HAL_NET_ENC_WEP) == 0) {
    ret = WIFI_AUTH_WEP;
  } else if (strcmp(encryption, HAL_NET_ENC_WPA_PSK) == 0) {
    ret = WIFI_AUTH_WPA_PSK;
  } else if (strcmp(encryption, HAL_NET_ENC_WPA2_PSK) == 0) {
    ret = WIFI_AUTH_WPA2_PSK;
  }
  return ret;
}

/**
 * @brief Set wifi station mode config.
 *
 * @code
 * hal_error err = hal_net_set_sta_config(ssid, password, encryption);
 * if (err != NULL) {
 *     return err
 * }
 * @endcode
 * @param ssid The ssid of station mode
 * @param password The password of station mode
 * @param encryption The encryption of station mode
 * @return The error code.
 */
hal_error hal_net_set_sta_config(const char* ssid, const char* password, const char* encryption)
{
  wifi_config_t wifi_config = {0};
  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.ssid));
  wifi_config.sta.threshold.authmode = hal_net_get_encryption(encryption);

  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }

  /** Make sure the config can be set without error */
  wifi_mode_t new_mode = mode | WIFI_MODE_STA;
  err = esp_wifi_set_mode(new_mode);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  /** Restore the mode */
  return hal_net_to_hal_error(esp_wifi_set_mode(mode));
}

/**
 * @brief Start wifi station mode.
 *
 * @return The error code.
 */
hal_error hal_net_start_sta(void)
{
  osal_mutex_lock(net->reconnect_lock);
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }
  mode |= WIFI_MODE_STA;
  ESP_LOGI(TAG, "Get mode");
  err = esp_wifi_stop();
  if (err != ESP_OK) {
    osal_mutex_unlock(net->reconnect_lock);
    return hal_net_to_hal_error(err);
  }
  ESP_LOGI(TAG, "Stop wifi");
  err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) {
    osal_mutex_unlock(net->reconnect_lock);
    return hal_net_to_hal_error(err);
  }
  ESP_LOGI(TAG, "Set wifi");
  err = esp_wifi_start();
  if (err != ESP_OK) {
    osal_mutex_unlock(net->reconnect_lock);
    return hal_net_to_hal_error(err);
  }
  ESP_LOGI(TAG, "Start wifi");
  osal_mutex_unlock(net->reconnect_lock);
  return HAL_RESULT_SUCCEED;
}

/**
 * @brief Stop wifi station mode.
 *
 * @return The error code.
 */
hal_error hal_net_stop_sta(void)
{
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }
  mode &= ~WIFI_MODE_STA;

  err = esp_wifi_stop();
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  return hal_net_to_hal_error(esp_wifi_start());
}

/**
 * @brief Set wifi ap mode config.
 *
 * @code
 * hal_error err = hal_net_set_ap_config(ssid, password, encryption);
 * if (err != NULL) {
 *     return err
 * }
 * @endcode
 * @param ssid The ssid of station mode
 * @param password The password of station mode
 * @param encryption The encryption of station mode
 * @return The error code.
 */
hal_error hal_net_set_ap_config(const char* ssid, const char* password, const char* encryption)
{
  wifi_config_t wifi_config = {0};
  strlcpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
  wifi_config.ap.ssid_len = strlen(ssid);
  if (password != NULL) strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.ssid));
  wifi_config.ap.max_connection = 10;
  wifi_config.ap.authmode = hal_net_get_encryption(encryption);

  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }

  /** Make sure the config can be set without error */
  wifi_mode_t new_mode = mode | WIFI_MODE_AP;
  err = esp_wifi_set_mode(new_mode);
  if (err != ESP_OK) return hal_net_to_hal_error(err);
  
  err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  /** Restore the mode */
  return hal_net_to_hal_error(esp_wifi_set_mode(mode));
}

/**
 * @brief Start wifi ap mode.
 *
 * @return The error code.
 */
hal_error hal_net_start_ap(void)
{
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }
  mode |= WIFI_MODE_AP;

  err = esp_wifi_stop();
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  return hal_net_to_hal_error(esp_wifi_start());
}

/**
 * @brief Stop wifi ap mode.
 *
 * @return The error code.
 */
hal_error hal_net_stop_ap(void)
{
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  if (err != ESP_OK) {
    mode = WIFI_MODE_NULL;
  }
  mode &= ~WIFI_MODE_AP;

  err = esp_wifi_stop();
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  return hal_net_to_hal_error(esp_wifi_start());
}

hal_error hal_net_stop()
{
  return hal_net_to_hal_error(esp_wifi_stop());
}

/**
 * @brief Get mac address array.
 *
 * @return A array contains mac address.
 */
uint8_t* hal_net_get_mac_address()
{
  uint8_t* mac = osal_mem_malloc(sizeof(uint8_t) * 6);
  esp_err_t err = esp_efuse_mac_get_default(mac);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot get default mac error:%d", err);
    osal_mem_free(mac);
    return NULL;
  }
  return mac;
}

/**
 * @brief Get mac address string.
 *
 * @return A string contains mac address with xx:xx:xx:xx:xx:xx format.
 */
char* hal_net_get_mac_address_string()
{
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  char* mac_str = osal_mem_malloc(18);
  sprintf(mac_str, MACSTR, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return mac_str;
}

/**
 * @brief Scan wifi ap.
 *
 * @param callback A callback which will be called when scanning is done.
 * @param user_data The user assigned data.
 * @return The error code.
 */
hal_error hal_net_start_scan(hal_net_on_scan callback, void* user_data)
{
  bool timeout = osal_mutex_lock_timeout(net->reconnect_lock, 5000);
  if (timeout) {
    return HAL_ERR_NET_WIFI_SCAN_WRONG_STATE;
  }

  if (net->scan_task == NULL)
  {
    net->scan_task = osal_task_create_timer((osal_timer_coroutine)callback, "scan_task", 1000, true);
    osal_task_start_timer(net->scan_task);
  }

  wifi_scan_config_t scan_config = {0};
  esp_err_t err = esp_wifi_scan_start(&scan_config, false);
  if (err != ESP_OK) {
    osal_mutex_unlock(net->reconnect_lock);
    return HAL_ERR_NET_WIFI_SCAN_WRONG_STATE;
  }
  osal_mutex_unlock(net->reconnect_lock);
  return HAL_RESULT_SUCCEED;
}

extern hal_error hal_net_stop_scan(void)
{
  if (net->scan_task != NULL)
  {
    osal_task_stop_timer(net->scan_task);
    net->scan_task = NULL;
  }

  esp_err_t err = esp_wifi_scan_stop();
  if (err == ESP_ERR_WIFI_NOT_INIT)
  {
    return HAL_ERR_NET_WIFI_NOT_INIT;
  }
  else if (err == ESP_ERR_WIFI_NOT_STARTED)
  {
    return HAL_ERR_NET_WIFI_SCAN_WRONG_STATE;
  }
  else
  {
    /* No action required */
  }
  return HAL_RESULT_SUCCEED;
}

void hal_net_set_on_sta_state_change_listener(hal_net_on_sta_state_change listener, void* user_data)
{
  if (listener == NULL) {
    net->on_sta_state_change_listener = on_sta_state_change_default;
    net->on_sta_state_change_user_data = NULL;
  } else {
    net->on_sta_state_change_listener = listener;
    net->on_sta_state_change_user_data = user_data;
  }
}

enum hal_net_sta_connect_error hal_net_get_sta_connect_error()
{
  return net->sta_connect_error;
}

enum hal_net_sta_state hal_net_get_sta_state()
{
  return net->sta_state;
}

hal_error hal_net_get_sta_info(struct hal_net_ap_data* data)
{
  wifi_ap_record_t esp_ap;
  esp_err_t err = esp_wifi_sta_get_ap_info(&esp_ap);
  if (err != ESP_OK) return hal_net_to_hal_error(err);

  memcpy(data->ssid, esp_ap.ssid, 32);
  data->ssid[32] = '\x0';
  data->rssi = esp_ap.rssi;
  data->encryption = hal_net_get_encryption_name(esp_ap.authmode);
  return HAL_RESULT_SUCCEED;
}

hal_error hal_net_ping(const char* ip, hal_net_on_ping_result callback)
{
  uint32_t ip_addr = ipaddr_addr(ip);
  uint32_t ping_count = 4;
  uint32_t ping_timeout = 1000;
  uint32_t ping_delay = 1000;

  net->on_ping_result = callback;

  ping_deinit();

  esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ping_count, sizeof(uint32_t));
  esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ping_timeout, sizeof(uint32_t));
  esp_ping_set_target(PING_TARGET_DELAY_TIME, &ping_delay, sizeof(uint32_t));
  esp_ping_set_target(PING_TARGET_IP_ADDRESS, &ip_addr, sizeof(uint32_t));

  esp_err_t ping_results(ping_target_id_t msgType, esp_ping_found * pf)
  {
    ESP_LOGI(TAG, "count:%d recv_count:%d timeout_count:%d time:%d ms", pf->send_count, pf->recv_count, pf->timeout_count, pf->resp_time);
    net->on_ping_result((struct hal_net_ping_result*)pf);
    return ESP_OK;
  }
  esp_ping_set_target(PING_TARGET_RES_FN, ping_results, sizeof(ping_results));
  int ping_err = ping_init();

  hal_error err = HAL_RESULT_SUCCEED;
  switch (ping_err) {
    case ERR_INPROGRESS:
      err = HAL_ERR_INVALID_STATE;
      break;
    case ERR_MEM:
      err = HAL_ERR_NO_MEM;
      break;
    default:
      err = HAL_RESULT_SUCCEED;
      break;
  }
  return err;
}
