#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>
#include "config/config.h"
#include "osal/times.h"
#include "osal/task.h"
#include "esp_log.h"
#include "sdk/wifi.h"
#include "sdk/WebSocket1.h"
#include "sdk/app.h"
#include "sdk/battery.h"
#include "hal/system.h"
#include <math.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ota headers start
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "driver/gpio.h"
//#include "protocol_examples_common.h"
#include "esp_system.h"
#include "errno.h"
//ota headers end

// http server headers start

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "ds1307.h"
#include "driver/ledc.h"

//http server headers end

#define TAG "MAIN"
#define SYSTEM_TIME_WAIT  (10*1000)
#define OFFSET_TIMESTAMP  1605708977// + 7*3600
#define DEFAULT_SLEEP     13
#define DEFAULT_START     0
#define DEFAULT_STOP      86399   // 24 hours on time

//ota defines start

#define BUFFSIZE 1024
#define HASH_LEN 32 
#define OTA_URL_SIZE 256

//ota defines end

// http server defines start
/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

#define SDA_GPIO 33
#define SCL_GPIO 34


// const int LED_OUTPUT_PIN = 21;

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (38) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (7000) // Frequency in Hertz. Set frequency at 5 kHz


ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_MODE,
    .timer_num        = LEDC_TIMER,
    .duty_resolution  = LEDC_DUTY_RES,
    .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
    .clk_cfg          = LEDC_AUTO_CLK
};



ledc_channel_config_t ledc_channel = {
    .speed_mode     = LEDC_MODE,
    .channel        = LEDC_CHANNEL,
    .timer_sel      = LEDC_TIMER,
    .intr_type      = LEDC_INTR_DISABLE,
    .gpio_num       = LEDC_OUTPUT_IO,
    .duty           = 0, // Set duty to 0%
    .hpoint         = 0
};


// http server defines end


extern void init_battery(void);
static void ota_example_task();
i2c_dev_t dev;

extern RTC_DATA_ATTR uint8_t app_today;
nvs_handle_t my_handle;
extern char *schedule[APP_NUMBER_OF_DAY];
extern RTC_DATA_ATTR app_scheduler_t m_scheduler;
RTC_DATA_ATTR uint8_t num_of_schedule = 0;

void erase_nvs_now(void);
EventGroupHandle_t event_group = NULL;

static const char *OTATAG = "native_ota_example";
static char ota_write_data[BUFFSIZE + 1] = { 0 };


bool MOTOR_PROCEED_ONLY = 0;       // To detect if proceed with only Motor or APP settings update

float battery_level;

typedef enum {
  WIFI_INTERFACE_IDLE = 0x00,
  WIFI_INTERFACE_CONNECT = 0x01,
  WIFI_INTERFACE_NUM = 0x02,
} WIFI_INTERFACE_STATE_E;

static WIFI_INTERFACE_STATE_E m_wifi_tcp_state;
static osal_task_handle tcp_handle;
static osal_task_handle system_handle;
time_t init_time = OFFSET_TIMESTAMP;

RTC_DATA_ATTR bool m_set_time = false;

void app_system_sleep(void* param)
{
  ESP_LOGI(TAG, "Move to sleep");
}


void app_demo_mode(void* param)
{
   
  while(1){
    xEventGroupWaitBits(event_group, DEMO_MODE_START, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Demo Mode Started.....");
    for (int count = 0; count <= 2; count++) {
      gpio_set_level(35, 1);
      ESP_LOGI(TAG, "Motor on! %d", count);
      vTaskDelay(470/portTICK_RATE_MS);
      gpio_set_level(35, 0);
      ESP_LOGI(TAG, "Motor off! %d", count);
      vTaskDelay(1530/portTICK_RATE_MS);
    }
    xEventGroupSetBits(event_group, DEMO_MODE_STOP);
  }
}

void app_feed_rtc(void* param)
{
  // ESP_LOGI(TAG,"Feeding the RTC");
  // struct timeval tv_init = {.tv_sec = init_time};
  // settimeofday(&tv_init, NULL);
  // 
  while(1){
    xEventGroupWaitBits(event_group, SET_TIME_STATE, pdTRUE, pdTRUE, portMAX_DELAY);
    // if (!m_set_time)
    // {
      ESP_LOGI(TAG,"Feeding the RTC");
      m_set_time = true;

      struct timeval tv_init = {.tv_sec = init_time};
      time_t time_e = (time_t)init_time;

      struct tm * new_time;
      new_time = gmtime (&time_e);

      ds1307_set_time(&dev, new_time);

      printf("%04d-%02d-%02d %02d:%02d:%02d\n", new_time->tm_year + 1900 /*Add 1900 for better readability*/, new_time->tm_mon + 1,
            new_time->tm_mday, new_time->tm_hour, new_time->tm_min, new_time->tm_sec);
      settimeofday(&tv_init, NULL);

      clock_settime(CLOCK_REALTIME, &tv_init);
    }
}


void app_setup_access_point(void)
{
  system_handle = osal_task_create_timer(app_system_sleep, "System Handle", SYSTEM_TIME_WAIT, false);
  osal_task_start_timer(system_handle);
}

void app_system_get_boot_reason(void)
{
  enum hal_system_wakeup_reason reason = hal_system_get_wakeup_reason();
  switch (reason)
  {
    case HAL_SYSTEM_WAKEUP_TIMER:
      ESP_LOGI(TAG, "Wakeup timer -> Update operation for Motor");
      MOTOR_PROCEED_ONLY = 1;
      m_set_time = true;
      break;
    case HAL_SYSTEM_WAKEUP_EXT0:
      ESP_LOGI(TAG, "Wakeup from IO -> Establish Access Point/TCP interafce");
      break;
    default:
      ESP_LOGI(TAG, "Wakeup from other source %d", reason);
      break;
  }

}

void close_nvs(void)
{
  nvs_close(my_handle);
  printf("Closing NVS\n");
}

void open_nvs(void)
{  
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);

  if (err != ESP_OK) {
      printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else
      printf("Done\n");

  return;
}

bool nvs_write_value(char *nvs_area,uint32_t nvs_value, int index)
{
  //vTaskDelay( 20 / portTICK_RATE_MS);
  //ESP_LOGI(TAG,"The area is :%s",nvs_area);
  //printf("Free memory %d \n",esp_get_free_heap_size());
  bool status = -1;
  esp_err_t err;

  open_nvs();
  size_t required_size = 120;  
  //printf("Enter Malloc\n");
  uint32_t* run_time = malloc(required_size);
  //printf("Max NVS %d\n", NVS_KEY_NAME_MAX_SIZE);

  // if(run_time!=NULL)
  // {
  //   printf("********Memory Allocated***********");
  // }
  // else
  // {
  //   printf("********Memory Not allocated***********");
  //   return 0;
  // }

  if(index == 0){
    memset(run_time, 0x00, required_size);
    err = nvs_set_blob(my_handle, nvs_area, run_time, required_size);

    if (err != ESP_OK) 
      //printf("Failed to write 3 \n"); 
      status = status;
  }



  // Read previously saved blob if available
  if (required_size > 0) {
      err = nvs_get_blob(my_handle, nvs_area, run_time, &required_size);
      if (err != ESP_OK) {
          //printf("get_bolb in nvs_write failed....");
          free(run_time);
          //printf("Failed to write 2 \n"); 
          return status;
      }
  }



  // Write value including previously saved blob if available
  //required_size += sizeof(uint32_t);
  //run_time[required_size / sizeof(uint32_t) - 1] = nvs_value;
  //memset(run_time, 0x00, strlen(run_time));
  run_time[index] = nvs_value;
  err = nvs_set_blob(my_handle, nvs_area, run_time, required_size);
  //printf("Exit Malloc\n");
  free(run_time);

  if (err != ESP_OK) {
    //printf("Failed to write\n");
    return status;
  }
  // Commit
  err = nvs_commit(my_handle);
  if (err != ESP_OK){
    //printf("Failed to write 1 \n"); 
    return status;
  }
  close_nvs();
  //printf("Free memory 2 %d \n",esp_get_free_heap_size());
  return 0;
}

void erase_nvs_now(void)
{
  nvs_flash_erase();
}

uint32_t* run_time;

bool nvs_read_data(char *nvs_area)
{


  bool status = -1;
  size_t required_size = 0;  // value will default to 0, if no-1 set yet in NVS
  
  open_nvs();
  // obtain required memory space to store blob being read from NVS
  esp_err_t err = nvs_get_blob(my_handle, nvs_area, NULL, &required_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return status;
  printf("Run time:\n");
  if (required_size == 0) {
      printf("Nothing saved yet!\n");
      return -1;
  } else {
      run_time = malloc(required_size);
      err = nvs_get_blob(my_handle, nvs_area, run_time, &required_size);
      if (err != ESP_OK) {
          free(run_time);
          return status;
      }
#ifdef DEBUG_MODE
      for (int i = 0; i < required_size / sizeof(uint32_t); i++) {
          printf("%d: %d\n", i + 1, run_time[i]);
      }
#endif
      //free(run_time);
  }
  close_nvs();
  return 0;
}


void backup_scheduler_now(void)
{

	ESP_LOGI(TAG, "Archiving schedule info to NVS memory........\n");

  // Example of nvs_get_stats() to get the number of used entries and free entries:
nvs_stats_t nvs_stats;
nvs_get_stats(NULL, &nvs_stats);
printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
       nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);

	
	for(int day=0; day < APP_NUMBER_OF_DAY; day++){
		nvs_write_value(schedule[day], m_scheduler.day[day].num, 0);
		ESP_LOGI(TAG, "Day %d has %d number of schedule", day, m_scheduler.day[day].num);
    int i = 0;


		for (int j=0; j < m_scheduler.day[day].num; j++)
		{
      ESP_LOGI(TAG, "%s start : %i \n",schedule[day],  m_scheduler.day[day].scheduler[j].start_time);
      ESP_LOGI(TAG, "%s stop : %i \n",schedule[day],  m_scheduler.day[day].scheduler[j].stop_time);
      ESP_LOGI(TAG, "%s interval_time : %i \n",schedule[day],  m_scheduler.day[day].scheduler[j].interval_time);
      ESP_LOGI(TAG, "%s index : %i \n",schedule[day],  m_scheduler.day[day].scheduler[j].index);
      if(m_scheduler.day[day].scheduler[j].active)ESP_LOGI(TAG, "%s active : true \n",schedule[day],  m_scheduler.day[day].scheduler[j].index);
      else ESP_LOGI(TAG, "%s active : false \n",schedule[day],  m_scheduler.day[day].scheduler[j].index);
      //ESP_LOGI(TAG,"%d  iteration",j);
      //if( j >2) continue;
			nvs_write_value(schedule[day], m_scheduler.day[day].scheduler[j].start_time, j+1+i);
			nvs_write_value(schedule[day], m_scheduler.day[day].scheduler[j].stop_time, j+2+i);
			nvs_write_value(schedule[day], m_scheduler.day[day].scheduler[j].interval_time, j+3+i);
      //ESP_LOGI(TAG,"stored 3 items");
			nvs_write_value(schedule[day], m_scheduler.day[day].scheduler[j].index, j+4+i);

      //ESP_LOGI(TAG,"***********************%d****************************",m_scheduler.day[day].scheduler[j].active);
      nvs_write_value(schedule[day], m_scheduler.day[day].scheduler[j].active, j+5+i);
      i+=5;
      //#ifdef DEBUG_MODE
			//#endif
		}

	}
  
  close_nvs();

}

void retreive_nvs_schedule(void)
{ 
  //schedule
  for(int day=0; day < APP_NUMBER_OF_DAY; day++){

    printf("Retreiving schedule for : %s\n",schedule[day]);
    bool status = nvs_read_data(schedule[day]);
    scheduler_t my_sche;
    int i = 0;
    if(status == 0){
      uint32_t size_blob = run_time[0];
      if(size_blob != 0)
      {
        for (int j=0; j < size_blob; j++)
        {
          my_sche.start_time = run_time[j+1+i];
          my_sche.stop_time = run_time[j+2+i];
          my_sche.interval_time = run_time[j+3+i];
          my_sche.index = run_time[j+4+i];
          my_sche.active=run_time[j+5+i];

          if(my_sche.index > num_of_schedule)
          {
            num_of_schedule = my_sche.index;
          }
          
          i += 5;
//#ifdef DEBUG_MODE
          printf("start_time : %i\n",my_sche.start_time);
          printf("stop_time : %i\n",my_sche.stop_time);
          printf("interval_time : %i\n",my_sche.interval_time);
          printf("index : %i\n",my_sche.index);
          if(my_sche.active)printf("active : true\n");
          else printf("active : false\n");
//#endif  
          if((my_sche.interval_time < 60))
            app_set_scheduler(day, my_sche);
        }
        free(run_time);
      }
    }
    else {
      printf("No Schedule in NVS memory...........\n");
    }
  }
  //backup_scheduler_now();
}


// http server functions start
/* Handler to upload a file onto the server */
/* Function to initialize SPIFFS */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,   // This decides the maximum number of files that can be created on the storage
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    } 

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

esp_err_t start_file_server(const char *base_path);


// http server functions end

void app_main(void)
{
  

ESP_LOGI(TAG,"********************TERRA LATEST 1.2.82 *********************************");
/* Initialize file storage */
ESP_ERROR_CHECK(init_spiffs());

retrive_gmt();

ESP_ERROR_CHECK(i2cdev_init());


gpio_set_direction(35, GPIO_MODE_OUTPUT);

ledc_timer_config(&ledc_timer);
ledc_channel_config(&ledc_channel);
ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
vTaskDelay(100/portTICK_RATE_MS);


memset(&dev, 0, sizeof(i2c_dev_t));
ds1307_init_desc(&dev, 0, SDA_GPIO, SCL_GPIO);
struct tm time_a;
bool is_i2c_running = false;

ds1307_is_running(&dev, &is_i2c_running);
time_t new_time_rtc;
if(is_i2c_running){
  ds1307_get_time(&dev, &time_a);
  printf("RTC is running\n");
  printf("%04d-%02d-%02d %02d:%02d:%02d\n", time_a.tm_year + 1900 /*Add 1900 for better readability*/, time_a.tm_mon + 1,
            time_a.tm_mday, time_a.tm_hour, time_a.tm_min, time_a.tm_sec);
  new_time_rtc = mktime ( &time_a );
}else{
  printf("RTC is not running\n");
}


// ESP_LOGI(TAG,"Feeding the RTC");
time_t now = time(NULL);
ESP_LOGI(TAG, "*************new rtc Epoch is %u*****************", now);
struct timeval tv_init = {.tv_sec = new_time_rtc};
settimeofday(&tv_init, NULL);

clock_settime(CLOCK_REALTIME, &tv_init);


uint32_t current_time = app_get_current_time();
ESP_LOGI(TAG, "**************  Current time: %d  **************", current_time);
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  init_battery();
  //battery_level = get_battery();
     if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      // NVS partition was truncated and needs to be erased
      // Retry nvs_flash_init
      ESP_LOGI(TAG, " Erasing the flash ");
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  app_set_old_millis();


  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
  //app_init_deep_sleep(  DEEP_SLEEP_TIMER , (120) * 1000);

  ESP_ERROR_CHECK( err );
  event_group = xEventGroupCreate();
  config_init();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  app_system_get_boot_reason();
  app_init();
  

  //app_get_current_time();




  /* Start the file server */

  battery_level = get_battery();

  ESP_LOGI(TAG," Battery level is : %.2f",battery_level);
   
  xTaskCreate(&app_demo_mode,"Demo_mode_task",3000,NULL,9,NULL);
  xTaskCreate(&app_feed_rtc,"Feed_rtc_time",3000,NULL,9,NULL);
  //xTaskCreate(&ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
 
  // app_system_get_boot_reason();

  retreive_nvs_schedule();

  if(MOTOR_PROCEED_ONLY != 1){

    wifi_init();
    start_websocket_server();     
     
    app_setup_access_point();
    ESP_ERROR_CHECK(start_file_server("/spiffs"));
    // wait for flag from webSocket if no activity or configuration completed 35 secs 
    xEventGroupWaitBits(event_group, PROCEED_STATE, pdTRUE, pdTRUE, portMAX_DELAY);

    //wifi_disconnect();
  }
  //app_arragne_scheduler_list();
  app_get_scheduler();
  ESP_LOGI(TAG, "Today is %i", app_today);
  app_init_scheduler();
  // xStart=xStop; /// reset the counter
  // ESP_ERROR_CHECK(example_connect());
  while(1)
  {
    ESP_LOGI(TAG, "Hello World");
    if (gpio_get_level(BUTTON_WAKEUP_GPIO) == 0) {

    	gpio_set_level(LED_GPIO, 1);

    }
    else
    {
    	gpio_set_level(LED_GPIO, 0);
    }
    vTaskDelay(1/portTICK_RATE_MS);
    // if the button is pressed
  

  }

}

