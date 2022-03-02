#include "hal/envs.h"
#include "hal/hal.h"
#include "esp_log.h"
#include "memory.h"

#define TAG "ENVS"

/**
 * @brief Intialize hal environment variables.
 *
 * This function must be called before you use any hal envs functions.
 */
void hal_envs_init(void)
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

static hal_error hal_envs_to_hal_error(esp_err_t esp_err)
{
  hal_error err;
  switch (esp_err) {
    case ESP_OK:
      err = HAL_RESULT_SUCCEED;
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      err = HAL_ERR_ENVS_NOT_FOUND;
      break;
    case ESP_ERR_NVS_INVALID_HANDLE:
      err = HAL_ERR_ENVS_INVALID_HANDLE;
      break;
    case ESP_ERR_NVS_INVALID_NAME:
      err = HAL_ERR_ENVS_INVALID_NAME;
      break;
    case ESP_ERR_NVS_INVALID_LENGTH:
      err = HAL_ERR_ENVS_INVALID_LENGTH;
      break;
    case ESP_ERR_NVS_READ_ONLY:
      err = HAL_ERR_ENVS_READ_ONLY;
      break;
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
      err = HAL_ERR_ENVS_NOT_ENOUGH_SPACE;
      break;
    case ESP_ERR_NVS_REMOVE_FAILED:
      err = HAL_ERR_ENVS_REMOVE_FAILED;
      break;
    case ESP_ERR_NVS_VALUE_TOO_LONG:
      err = HAL_ERR_ENVS_VALUE_TOO_LONG;
      break;
    case ESP_ERR_NVS_NOT_INITIALIZED:
      err = HAL_ERR_ENVS_NOT_INITIALIZED;
      break;
    case ESP_ERR_NVS_PART_NOT_FOUND:
      err = HAL_ERR_ENVS_PART_NOT_FOUND;
      break;
    default:
      ESP_LOGE(TAG, "unknow esp error: %d", esp_err);
      err = HAL_ERR;
  }
  return err;
}

/**
 * @brief Open a specific table.
 *
 * @param name the table name which you want to get
 */
struct hal_envs* hal_envs_open(const char* name)
{
  struct hal_envs* envs = (struct hal_envs*)osal_mem_malloc(sizeof(struct hal_envs));
  esp_err_t err = nvs_open(name, NVS_READWRITE, &envs->nvs);
  if (err != ESP_OK) {
    osal_mem_free(envs);
    return NULL;
  }
  return envs;
}

/**
 * @brief Retrieve a int8 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
hal_error hal_envs_get_i8(struct hal_envs* envs, const char* key, int8_t* value)
{
  return hal_envs_to_hal_error(nvs_get_i8(envs->nvs, key, value));
}

/**
 * @brief Retrieve a unsigned int8 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
hal_error hal_envs_get_u8(struct hal_envs* envs, const char* key, uint8_t* value)
{
  return hal_envs_to_hal_error(nvs_get_u8(envs->nvs, key, value));
}

/**
 * @brief Retrieve a int32 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
hal_error hal_envs_get_i32(struct hal_envs* envs, const char* key, int32_t* value)
{
  return hal_envs_to_hal_error(nvs_get_i32(envs->nvs, key, value));
}

/**
 * @brief Retrieve a unsigned int32 value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @param value The stored value.
 * @return The error code.
 */
hal_error hal_envs_get_u32(struct hal_envs* envs, const char* key, uint32_t* value)
{
  return hal_envs_to_hal_error(nvs_get_u32(envs->nvs, key, value));
}

/**
 * @brief Retrieve a string value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @return The stored value or NULL if the value is not exist.
 */
char* hal_envs_get_str(struct hal_envs* envs, const char* key)
{
  size_t size = 0;
  esp_err_t err = nvs_get_str(envs->nvs, key, NULL, &size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot get key:%s error:%d", key, err);
    return NULL;
  }

  char* value = (char*)osal_mem_malloc(size);
  err = nvs_get_str(envs->nvs, key, value, &size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot get key:%s error:%d", key, err);
    return NULL;
  }
  return value;
}

/**
 * @brief Retrieve a blob value from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to get
 * @return The stored value or NULL if the value is not exist.
 */
void* hal_envs_get_blob(struct hal_envs* envs, const char* key)
{
  size_t size = 0;
  esp_err_t err = nvs_get_blob(envs->nvs, key, NULL, &size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot get key:%s error:%d", key, err);
    return NULL;
  }

  void* value = (void*)osal_mem_malloc(size);
  err = nvs_get_blob(envs->nvs, key, value, &size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot get key:%s error:%d", key, err);
    return NULL;
  }
  return value;
}

/**
 * @brief Set a int8 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_i8(struct hal_envs* envs, const char* key, int8_t value)
{
  return hal_envs_to_hal_error(nvs_set_i8(envs->nvs, key, value));
}

/**
 * @brief Set a unsigned int8 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_u8(struct hal_envs* envs, const char* key, uint8_t value)
{
  return hal_envs_to_hal_error(nvs_set_u8(envs->nvs, key, value));
}

/**
 * @brief Set a int32 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_i32(struct hal_envs* envs, const char* key, int32_t value)
{
  return hal_envs_to_hal_error(nvs_set_i32(envs->nvs, key, value));
}

/**
 * @brief Set a unsigned int32 value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_u32(struct hal_envs* envs, const char* key, uint32_t value)
{
  return hal_envs_to_hal_error(nvs_set_u32(envs->nvs, key, value));
}

/**
 * @brief Set a string value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_str(struct hal_envs* envs, const char* key, const char* value)
{
  return hal_envs_to_hal_error(nvs_set_str(envs->nvs, key, value));
}

/**
 * @brief Set a blob value to the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to set
 * @param value The value which you want to set
 * @return The error code.
 */
hal_error hal_envs_set_blob(struct hal_envs* envs, const char* key, const void* value, size_t length)
{
  return hal_envs_to_hal_error(nvs_set_blob(envs->nvs, key, value, length));
}

/**
 * @brief Erase a key from the hal environment variables.
 *
 * @param envs The handle of specific table
 * @param key The key which you want to erase
 * @return The error code.
 */
hal_error hal_envs_erase_key(struct hal_envs* envs, const char* key)
{
  esp_err_t err = nvs_erase_key(envs->nvs, key);
  if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
  return hal_envs_to_hal_error(err);
}

/**
 * @brief Save the changes of the environment variables to storage.
 *
 * @param envs The handle of specific table
 * @return The error code.
 */
hal_error hal_envs_save(struct hal_envs* envs)
{
  return hal_envs_to_hal_error(nvs_commit(envs->nvs));
}

