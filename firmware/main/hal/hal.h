#ifndef HAL_H
#define HAL_H


typedef int hal_error;

#define HAL_RESULT_SUCCEED    (0)
#define HAL_ERR               (-1)

#define HAL_ERR_INVALID_INPUT (1)
#define HAL_ERR_INVALID_ARG   (2)
#define HAL_ERR_NO_MEM        (3)
#define HAL_ERR_NOT_SUPPORTED (4)
#define HAL_ERR_NOT_FOUND     (5)
#define HAL_ERR_INVALID_SIZE  (6)
#define HAL_ERR_INVALID_STATE (7)

#define HAL_ERR_ENVS_BASE             (0x1100)
#define HAL_ERR_ENVS_NOT_FOUND        (HAL_ERR_ENVS_BASE + 1)
#define HAL_ERR_ENVS_INVALID_HANDLE   (HAL_ERR_ENVS_BASE + 2)
#define HAL_ERR_ENVS_INVALID_NAME     (HAL_ERR_ENVS_BASE + 3)
#define HAL_ERR_ENVS_INVALID_LENGTH   (HAL_ERR_ENVS_BASE + 4)
#define HAL_ERR_ENVS_READ_ONLY        (HAL_ERR_ENVS_BASE + 5)
#define HAL_ERR_ENVS_NOT_ENOUGH_SPACE (HAL_ERR_ENVS_BASE + 6)
#define HAL_ERR_ENVS_REMOVE_FAILED    (HAL_ERR_ENVS_BASE + 7)
#define HAL_ERR_ENVS_VALUE_TOO_LONG   (HAL_ERR_ENVS_BASE + 8)
#define HAL_ERR_ENVS_NOT_INITIALIZED  (HAL_ERR_ENVS_BASE + 9)
#define HAL_ERR_ENVS_PART_NOT_FOUND   (HAL_ERR_ENVS_BASE + 10)

#define HAL_ERR_NET_BASE                   (0x1200)
#define HAL_ERR_NET_WIFI_NOT_INIT          (HAL_ERR_NET_BASE + 1)
#define HAL_ERR_NET_WIFI_INVALID_INTERFACE (HAL_ERR_NET_BASE + 2)
#define HAL_ERR_NET_WIFI_INVALID_MODE      (HAL_ERR_NET_BASE + 3)
#define HAL_ERR_NET_WIFI_INVALID_PASSWORD  (HAL_ERR_NET_BASE + 4)
#define HAL_ERR_NET_WIFI_INVALID_ENVS      (HAL_ERR_NET_BASE + 5)
#define HAL_ERR_NET_WIFI_CONN              (HAL_ERR_NET_BASE + 6)
#define HAL_ERR_NET_WIFI_SCAN_WRONG_STATE  (HAL_ERR_NET_BASE + 7)
#define HAL_ERR_NET_WIFI_NOT_CONNECT       (HAL_ERR_NET_BASE + 8)

#define HAL_ERR_OTA_BASE                   (0x1500)                    /**< Base error code for ota_ops api */
#define HAL_ERR_OTA_PARTITION_CONFLICT     (HAL_ERR_OTA_BASE + 0x01)  /**< Error if request was to write or erase the current running partition */
#define HAL_ERR_OTA_SELECT_INFO_INVALID    (HAL_ERR_OTA_BASE + 0x02)  /**< Error if OTA data partition contains invalid content */
#define HAL_ERR_OTA_VALIDATE_FAILED        (HAL_ERR_OTA_BASE + 0x03)  /**< Error if OTA app image is invalid */
#define HAL_ERR_OTA_SMALL_SEC_VER          (HAL_ERR_OTA_BASE + 0x04)  /**< Error if the firmware has a secure version less than the running firmware. */
#define HAL_ERR_OTA_ROLLBACK_FAILED        (HAL_ERR_OTA_BASE + 0x05)  /**< Error if flash does not have valid firmware in passive partition and hence rollback is not possible */
#define HAL_ERR_OTA_ROLLBACK_INVALID_STATE (HAL_ERR_OTA_BASE + 0x06)  /**< Error if current active firmware is still marked in pending validation state (HAL_OTA_STATE_PENDING_VERIFY), essentially first boot of firmware image post upgrade and hence firmware upgrade is not possible */
#define HAL_ERR_OTA_SAME_VERSION           (HAL_ERR_OTA_BASE + 0x07)

#define HAL_ERR_FLASH_BASE       (0x10010)
#define HAL_ERR_FLASH_OP_FAIL    (HAL_ERR_FLASH_BASE + 1)
#define HAL_ERR_FLASH_OP_TIMEOUT (HAL_ERR_FLASH_BASE + 2)

#define HAL_ERR_SPI_BASE          (0x1A00)
#define HAL_ERR_SPI_FAIL_INIT     (HAL_ERR_SPI_BASE + 1)
#define HAL_ERR_SPI_FAIL_TRANSMIT (HAL_ERR_SPI_BASE + 2)
#define HAL_ERR_SPI_FAIL_TRANSFER (HAL_ERR_SPI_BASE + 3)
#define HAL_ERR_SPI_FAIL_RECEIVE  (HAL_ERR_SPI_BASE + 4)

#endif /* HAL_H */
