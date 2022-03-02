#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>

#include "hal.h"
#include "osal/event.h"
#include "osal/mutex.h"
#include "osal/task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#define HAL_NET_ENC_NONE            "none"
#define HAL_NET_ENC_WEP             "wep"
#define HAL_NET_ENC_WPA_PSK         "wpa-psk"
#define HAL_NET_ENC_WPA2_PSK        "wpa2-psk"

#define HAL_NET_MAX_RECONNECT_TIMEOUT 60


enum hal_net_sta_state {
    HAL_NET_STA_STATE_DISCONNECTED = 0,
    HAL_NET_STA_STATE_CONNECTING,
    HAL_NET_STA_STATE_RECONNECTING,
    HAL_NET_STA_STATE_CONNECTED_WITHOUT_IP,
    HAL_NET_STA_STATE_CONNECTED,
};

typedef void (*hal_net_on_sta_state_change)(enum hal_net_sta_state old_state, enum hal_net_sta_state new_state, void* user_data);

enum hal_net_sta_connect_error {
    HAL_NET_STA_CONNECT_ERROR_AP_NOT_FOUND = 0,
    HAL_NET_STA_CONNECT_ERROR_INVALID_PASSWORD,
    HAL_NET_STA_CONNECT_ERROR_CANNOT_GET_IP,
    HAL_NET_STA_CONNECT_ERROR_NONE,
};

/**
 * @brief A struct containing ap data.
 */
struct hal_net_ap_data {
    char ssid[33]; /**< @brief The ssid name */
    int8_t rssi; /**< @brief The db of rssi */
    const char* encryption; /**< @brief The encryption */
};

/**
 * @brief A callback which will be called when scanning is done.
 *
 * @param aps The ap array
 * @param user_data The user assigned data
 */
typedef void (*hal_net_on_scan)(void);

struct hal_net_ping_result {
    uint32_t resp_time;
    uint32_t timeout_count;
    uint32_t send_count;
    uint32_t recv_count;
    uint32_t err_count;
    uint32_t bytes;
    uint32_t total_bytes;
    uint32_t total_time;
    uint32_t min_time;
    uint32_t max_time;
    int8_t   ping_err;
};

/**
 * @brief A callback which will be called when ping is done.
 *
 * @param result The ping result
 */
typedef void (*hal_net_on_ping_result)(struct hal_net_ping_result* result);

/**
 * @brief Intialize hal network.
 *
 * This function must be called before you use any hal network APIs.
 */
extern hal_error hal_net_init(void);

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
extern hal_error hal_net_set_sta_config(const char* ssid, const char* password, const char* encryption);

/**
 * @brief Start wifi station mode.
 *
 * @return The error code.
 */
extern hal_error hal_net_start_sta(void);

/**
 * @brief Stop wifi station mode.
 *
 * @return The error code.
 */
extern hal_error hal_net_stop_sta(void);

/**
 * @brief Set wifi ap mode config.
 *
 * @code
 * hal_error err = hal_net_set_ap_config(ssid, password, encryption);
 * if (err != NULL) {
 *     return err
 * }
 * @endcode
 * @param ssid The ssid of access point mode
 * @param password The password of access point mode
 * @param encryption The encryption of access point mode
 * @return The error code.
 */
extern hal_error hal_net_set_ap_config(const char* ssid, const char* password, const char* encryption);

/**
 * @brief Start wifi ap mode.
 *
 * @return The error code.
 */
extern hal_error hal_net_start_ap(void);

/**
 * @brief Stop wifi ap mode.
 *
 * @return The error code.
 */
extern hal_error hal_net_stop_ap(void);

/**
 * @brief Get mac address array.
 *
 * @return A array contains mac address.
 */
extern uint8_t* hal_net_get_mac_address();

/**
 * @brief Get mac address string.
 *
 * @return A string contains mac address with xx:xx:xx:xx:xx:xx format.
 */
extern char* hal_net_get_mac_address_string();

/**
 * @brief Scan wifi ap.
 *
 * @param callback A callback which will be called when scanning is done.
 * @param user_data The user assigned data.
 * @return The error code.
 */
extern hal_error hal_net_start_scan(hal_net_on_scan callback, void* user_data);

extern hal_error hal_net_stop_scan(void);

extern enum hal_net_sta_state hal_net_get_sta_state();

extern void hal_net_set_on_sta_state_change_listener(hal_net_on_sta_state_change listener, void* user_data);

extern enum hal_net_sta_connect_error hal_net_get_sta_connect_error();

extern hal_error hal_net_get_sta_info(struct hal_net_ap_data* data);

extern hal_error hal_net_ping(const char* ip, hal_net_on_ping_result callback);

#endif /* HAL_NETWORK_H */
