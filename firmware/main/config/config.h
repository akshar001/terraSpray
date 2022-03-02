#ifndef CONFIG_CONFIG_H_
#define CONFIG_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_system.h"

#define CFG_MAGIC_FIRST_BOOT           0xFA
#define CFG_DEFAULT_SSID               "Scent+Spray"
#define CFG_DEFAULT_PASSWORD           "12345678"
#define CFG_STR_LEN                    128

typedef enum {
  CFG_FIRST_BOOT = 0x00,
  CFG_NETWORK_SSID,
  CFG_NETWORK_PASS,
  CFG_DATA_NUM
} cfg_element_e;

typedef union {
  uint8_t  u8;
  int8_t   s8;
  uint16_t u16;
  int16_t  s16;
  uint32_t u32;
  int32_t  s32;
  char     str[CFG_STR_LEN];
} cfg_data;

typedef enum {
  CFG_U8,
  CFG_S8,
  CFG_U16,
  CFG_S16,
  CFG_U32,
  CFG_S32,
  CFG_STR,
} cfg_data_type_e;

/**
 * @brief Configuration initialization
 *
 *
 * @param None
 * @return None
 */
void config_init(void);

/**
 * @brief Configuration load
 *
 *
 * @param None
 * @return None
 */
void config_load(void);

/**
 * @brief Set configuration data
 *
 * @param element Element of configuration data to set
 * @param type type of data such as U8, S8 etc..
 * @param value to set data
 *
 * @return The error code.
 */
esp_err_t config_set_data(cfg_element_e element, cfg_data_type_e type, cfg_data data);

/**
 * @brief Get configuration data
 *
 * @param element Element of configuration data to set
 * @param type type of data such as U8, S8 etc..
 * @param value pointer to read data
 *
 * @return The error code.
 */
esp_err_t config_get_data(cfg_element_e element, cfg_data_type_e type, cfg_data *value);

esp_err_t config_set_u32(const char* tag, uint32_t value);
esp_err_t config_get_u32(const char* tag, uint32_t* value);

#endif /* CONFIG_CONFIG_H_ */
