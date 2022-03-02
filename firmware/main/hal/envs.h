/**
 * @file
 *
 * Initialize environment variables
 * @code
 * hal_envs_init();
 * @endcode
 * Get environment variables
 * @code
 * struct hal_envs* envs = hal_envs_open(ENVS_TABLE_WIFI);
 * char* ssid = hal_envs_get_str(envs, ENVS_WIFI_KEY_STA_SSID);
 * if (ssid != NULL) {
 *     ...
 *     osal_mem_free(ssid)
 * }
 * @endcode
 * Set environment variables
 * @code
 * struct hal_envs* envs = hal_envs_open(ENVS_TABLE_WIFI);
 * hal_error err = hal_envs_set_str(envs, ENVS_WIFI_KEY_STA_SSID, ssid);
 * if (err != HAL_RESULT_SUCCEED) {
 *     return err;
 * }
 *
 * hal_error err = hal_envs_set_str(envs, ENVS_WIFI_KEY_STA_PASS, password);
 * if (err != HAL_RESULT_SUCCEED) {
 *     return err;
 * }
 *
 * hal_error err = hal_envs_set_str(envs, ENVS_WIFI_KEY_STA_ENC, encryption);
 * if (err != HAL_RESULT_SUCCEED) {
 *     return err;
 * }
 *
 * hal_error err = hal_envs_save(envs);
 * if (err != HAL_RESULT_SUCCEED) {
 *     return err;
 * }
 *
 * @endcode
 */

#ifndef HAL_ENVS_H
#define HAL_ENVS_H

#include <stdint.h>
#include "nvs_flash.h"
#include "hal/hal.h"


/**
 * @brief A handle for a specific table of environment variables.
 */
struct hal_envs {
    nvs_handle nvs;
};

/**
 * @brief Intialize hal environment variables.
 *
 * This function must be called before you use any hal envs APIs.
 */
void hal_envs_init(void);

/**
 * @brief Open a specific table.
 *
 * @param name the table name which you want to get
 */
extern struct hal_envs* hal_envs_open(const char* name);

/**
 * @brief Retrieve a int8 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
extern hal_error hal_envs_get_i8(struct hal_envs* envs, const char* key, int8_t* value);

/**
 * @brief Retrieve a unsigned int8 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
extern hal_error hal_envs_get_u8(struct hal_envs* envs, const char* key, uint8_t* value);

/**
 * @brief Retrieve a int32 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
extern hal_error hal_envs_get_i32(struct hal_envs* envs, const char* key, int32_t* value);

/**
 * @brief Retrieve a unsigned int32 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
extern hal_error hal_envs_get_u32(struct hal_envs* envs, const char* key, uint32_t* value);

/**
 * @brief Retrieve a string value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @return The stored value or NULL if the value is not exist.
 */
extern char* hal_envs_get_str(struct hal_envs* envs, const char* key);

/**
 * @brief Retrieve a blob value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @return The stored value or NULL if the value is not exist.
 */
extern void* hal_envs_get_blob(struct hal_envs* envs, const char* key);

/**
 * @brief Set a int8 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_i8(struct hal_envs* envs, const char* key, int8_t value);

/**
 * @brief Set a unsigned int8 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_u8(struct hal_envs* envs, const char* key, uint8_t value);

/**
 * @brief Set a int32 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_i32(struct hal_envs* envs, const char* key, int32_t value);

/**
 * @brief Set a unsigned int32 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_u32(struct hal_envs* envs, const char* key, uint32_t value);

/**
 * @brief Set a string value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_str(struct hal_envs* envs, const char* key, const char* value);

/**
 * @brief Set a blob value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
extern hal_error hal_envs_set_blob(struct hal_envs* envs, const char* key, const void* value, size_t length);

/**
 * @brief Erase a key from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to erase
 * @return The error code.
 */
extern hal_error hal_envs_erase_key(struct hal_envs* envs, const char* key);

/**
 * @brief Save the changes of the environment variables to storage.
 *
 * @param envs The handle of specific table
 * @return The error code.
 */
extern hal_error hal_envs_save(struct hal_envs* envs);

#endif /* HAL_ENVS_H */
