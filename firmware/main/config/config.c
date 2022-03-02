#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "config.h"
#include "esp_log.h"
#include "envs.h"
#include "osal/memory.h"

#define STORAGE_NAMESPACE "global"
#define TAG "CONFIG"

static const char* config_global_list[CFG_DATA_NUM] = {
    "first_boot",
    "ssid",
    "password"
};

static struct hal_envs* envs;

void config_init(void)
{
  hal_envs_init();

  envs = hal_envs_open(STORAGE_NAMESPACE);
  if (NULL == envs)
  {
    ESP_LOGE(TAG, "Failed to hal_envs_open\n");
    return;
  }

  config_load();
}

void config_load(void)
{
  cfg_data data;
  // if (config_get_data(CFG_FIRST_BOOT, CFG_U8, &data) == ESP_OK)
  // {
  //   if (data.u8 == CFG_MAGIC_FIRST_BOOT)
  //   {
  //     ESP_LOGI(TAG, "Passed CFG_MAGIC_FIRST_BOOT");
  //     {
  //       memset(data.str, '\0', CFG_STR_LEN * sizeof(char));
  //       config_get_data(CFG_NETWORK_SSID, CFG_STR, &data);
  //       ESP_LOGI(TAG, "SSID %s", data.str);
  //     }
  //     {
  //       memset(data.str, '\0', CFG_STR_LEN * sizeof(char));
  //       config_get_data(CFG_NETWORK_PASS, CFG_STR, &data);
  //       ESP_LOGI(TAG, "Password ID %s", data.str);
  //     }
  //     return;
  //   }
  // }
  // else
  // {
    {
      data.u8 = CFG_MAGIC_FIRST_BOOT;
      config_set_data(CFG_FIRST_BOOT, CFG_U8, data);
    }
    {
      memset(data.str, '\0', CFG_STR_LEN * sizeof(char));
      sprintf(data.str, "%s", CFG_DEFAULT_SSID);
      data.str[strlen((const char*)CFG_DEFAULT_SSID)] = '\0';
      config_set_data(CFG_NETWORK_SSID, CFG_STR, data);
    }
    {
      memset(data.str, '\0', CFG_STR_LEN * sizeof(char));
      sprintf(data.str, "%s", CFG_DEFAULT_PASSWORD);
      data.str[strlen((const char*)CFG_DEFAULT_PASSWORD)] = '\0';
      config_set_data(CFG_NETWORK_PASS, CFG_STR, data);
    }
  // }
}

esp_err_t config_set_data(cfg_element_e element, cfg_data_type_e type, cfg_data data)
{
  if (envs == NULL) return ESP_FAIL;
  hal_error error = HAL_RESULT_SUCCEED;
  switch (type)
  {
    case CFG_U8:
      error = hal_envs_set_u8(envs, config_global_list[element], data.u8);
      break;
    case CFG_S8:
      break;
    case CFG_U16:
      break;
    case CFG_S16:
      break;
    case CFG_U32:
      error = hal_envs_set_u32(envs, config_global_list[element], data.u32);
      break;
    case CFG_S32:
      break;
    case CFG_STR:
      if (data.str[0] == '\0')
      {
        error = hal_envs_erase_key(envs, config_global_list[element]);
      }
      else
      {
        error = hal_envs_set_str(envs, config_global_list[element], data.str);
      }
      break;
    default: break;
  }

  if (error != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to set value type %d", type);
    return ESP_FAIL;
  }

  error = hal_envs_save(envs);
  if (error != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to save value envs %d", type);
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t config_get_data(cfg_element_e element, cfg_data_type_e type, cfg_data *data)
{
  if (envs == NULL) return ESP_FAIL;
  hal_error error = HAL_RESULT_SUCCEED;
  switch (type)
  {
    case CFG_U8:
      error = hal_envs_get_u8(envs, config_global_list[element], &data->u8);
      break;
    case CFG_S8:
      break;
    case CFG_U16:
      break;
    case CFG_S16:
      break;
    case CFG_U32:
      error = hal_envs_get_u32(envs, config_global_list[element], &data->u32);
      break;
    case CFG_S32:
      break;
    case CFG_STR:
    {
      char* response = hal_envs_get_str(envs, config_global_list[element]);
      if (response != NULL)
      {
        strncpy(data->str, response, strlen(response));
        osal_mem_free(response);
      }
      else
      {
        error = HAL_ERR;
      }
      break;
    }
    default:
      break;
  }

  if (error != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to get value envs %d", type);
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t config_set_u32(const char* tag, uint32_t value)
{
  if (envs == NULL) return ESP_FAIL;
  hal_error error = HAL_RESULT_SUCCEED;
  error = hal_envs_set_u32(envs, tag, value);
  if (error != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to set value tag %s", tag);
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t config_get_u32(const char* tag, uint32_t* value)
{
  if (envs == NULL) return ESP_FAIL;
  hal_error error = HAL_RESULT_SUCCEED;
  error = hal_envs_get_u32(envs, tag, value);
  if (error != HAL_RESULT_SUCCEED)
  {
    ESP_LOGE(TAG, "Failed to get value tag %s", tag);
    return ESP_FAIL;
  }

  return ESP_OK;
}
