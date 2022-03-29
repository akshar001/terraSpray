/*
An example showing the ESP32 as a
WebSocket server.

Demonstrates:
the ESP32 as a WiFi Access Point,
embedding files (html, js, etc.) for a server,
WebSockets.

All WebSocket Server options are in:
Component config ---> WebSocket Server

Connect to the Access Point,
default name: "TerraDev"
password: "12345678"

go to 192.168.1.10 in a browser

Note that there are two regular server tasks.
The first gets incoming clients, then passes them
to a queue for the second task to actually handle.
I found that connections were dropped much less frequently
this way, especially when handling large requests. It
does use more RAM though.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/api.h"

#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/ledc.h"

#include "string.h"
#include "cJSON.h"

#include "websocket_server.h"
#include "WebSocket1.h"
#include "sdk/app.h"
#include<math.h>

#define MY_PORT  12345
#define WAIT_TIME_APP   7000    

#define VERSION "V1.2.83"
bool xTaskProceeed = 0;
int num_clinet = -1;
int start_in = 1;

extern float battery_level;
extern  RTC_DATA_ATTR char print_time_zone[100];
extern RTC_DATA_ATTR char time_zone[100];
extern RTC_DATA_ATTR uint8_t num_of_schedule;

extern RTC_DATA_ATTR app_scheduler_t m_scheduler;
extern RTC_DATA_ATTR uint32_t spray_count;

extern void app_remove_scheduler_pass(uint8_t index_del);
extern int get_battery(void);
extern void backup_scheduler_now(void);
extern bool nvs_write_value(char *nvs_area,uint32_t nvs_value, int index);
extern bool nvs_read_data(char *nvs_area);
extern uint32_t* run_time;
extern RTC_DATA_ATTR uint32_t remaining_sprays;

static QueueHandle_t client_queue;
const static int client_queue_size = 10;
uint32_t xStart = 0, xStop = 0;

enum data_type{
    NONE = 0,
    SET_SCHEDULER,
    SET_INFO,
    SET_CONFIG,
    GET_SCHEDULER,
    DEL_SCHEDULE,
    OTHER_STATE,
    GET_SNAPSHOT
};

uint8_t get_current_state(char* msg,uint64_t len);

int current_state = NONE;
char* days[]={"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

cJSON *data_info;
bool dat_req_rec = false;
bool scheduler_set = false;

RTC_DATA_ATTR uint16_t initialize_spary;
RTC_DATA_ATTR uint32_t days_left = 0;
RTC_DATA_ATTR char Disp_type[100];
          
int schedule_count = 5;
int start_1 = 123;
int stop_1 = 321;
int interval = 12;
char *day = "Monday";
char* active_status;

extern time_t init_time;


void swap_items(scheduler_t *xp, scheduler_t *yp)
{

    scheduler_t temp = *xp;
    *xp = *yp;
    *yp = temp;
}


void sort_timeline_array(scheduler_t* schedule_array,int size)
{

  int i, j;
    for (i = 0; i < size-1; i++)
    {    
     
    for (j = 0; j < size-i-1; j++)
    {
        if (schedule_array[j].start_time > schedule_array[j+1].start_time)
        {
            swap(&schedule_array[j], &schedule_array[j+1]);
        }


        else if(schedule_array[j].start_time==schedule_array[j+1].start_time)
        {
          if (schedule_array[j].stop_time < schedule_array[j+1].stop_time)
              swap(&schedule_array[j], &schedule_array[j+1]);

        } 

    }

  }

}

uint32_t count_spray(scheduler_t* todays_schedule,uint8_t active_count)
{

  sort_schedule(todays_schedule,active_count);

  for(int i=0;i<active_count;i++)
  {
    ESP_LOGI("WEBSOCKET","Starttime: %d  Stoptime: %d",todays_schedule[i].start_time,todays_schedule[i].stop_time);
  }

  uint32_t runtime,spray_count=0,total_spray_count=0;

  for(int i=0;i<active_count;i++)
  {
    scheduler_t parent_scheduler;

    parent_scheduler = todays_schedule[i];

    if(parent_scheduler.start_time==0 && parent_scheduler.stop_time==0)
    {
      continue;

    }

    if(!parent_scheduler.active)
    {
      continue;
    }

    ESP_LOGI("WEBSOCKET","Parent Scheduler Starttime: %d  Stoptime: %d",parent_scheduler.start_time,parent_scheduler.stop_time);

    runtime= parent_scheduler.stop_time - parent_scheduler.start_time;

    ESP_LOGI("WEBOCKET","Actual Run time of %d scheduler is : %d",i,runtime);

    int it = (i+1)%active_count;
    int child_schedule_run_time=0;

    do
    {
      if(parent_scheduler.start_time<=todays_schedule[it].start_time && parent_scheduler.stop_time>todays_schedule[it].stop_time && todays_schedule[it].active)
      {
         ESP_LOGI("WEBSOCKET","Child schedule Starttime: %d  Stoptime: %d",todays_schedule[it].start_time,todays_schedule[it].stop_time);

          child_schedule_run_time = todays_schedule[it].stop_time - todays_schedule[it].start_time;
          ESP_LOGI("WEBSOCKET","Child Schedule Run time %d",child_schedule_run_time);
          runtime-=child_schedule_run_time;

      }
      else if(parent_scheduler.start_time<todays_schedule[it].start_time && todays_schedule[it].start_time < parent_scheduler.stop_time && parent_scheduler.stop_time<todays_schedule[it].stop_time && todays_schedule[it].active)
      {

         ESP_LOGI("WEBSOCKET","Child schedule Starttime: %d  Stoptime: %d",todays_schedule[it].start_time,todays_schedule[it].stop_time);

          child_schedule_run_time = parent_scheduler.stop_time - todays_schedule[it].start_time;
          ESP_LOGI("WEBSOCKET","Child Schedule Run time %d",child_schedule_run_time);
          runtime-=child_schedule_run_time;

      }

      it=(it+1)%active_count;

    }while(it!=i);

    ESP_LOGI("WEBOCKET","Run time of %d scheduler is : %d",i,runtime);

    uint32_t parent_interval = parent_scheduler.interval_time*60;

    if(runtime%parent_interval==0)
      spray_count = runtime/parent_interval;
    else
      spray_count = (runtime/parent_interval)+1;

    ESP_LOGI("WEBSOCKET","Total Spray_count: %d",spray_count);
    total_spray_count+=spray_count;

  }

  ESP_LOGI("WEBSOCKET","sprays used by this scheduler: %d",total_spray_count);
  return total_spray_count;

}

static void removeSpaces(char *str) 
{ 
    // To keep track of non-space character count 
    int count = 0; 
  
    // Traverse the given string. If current character 
    // is not space, then place it at index 'count++' 
    for (int i = 0; str[i]; i++) 
      if (str[i] != ' ') 
          str[count++] = str[i]; // here count is = // incremented 
    str[count] = '\0'; 
} 

// Create array
cJSON *Create_array_of_anything(cJSON **objects,int array_num)
{
	cJSON *prev = 0;
	cJSON *root;
	root = cJSON_CreateArray();
	for (int i=0;i<array_num;i++) {
		if (!i)	{
			root->child=objects[i];
		} else {
			prev->next=objects[i];
			objects[i]->prev=prev;
		}
		prev=objects[i];
	}
	return root;
}

void update_scheduler(uint8_t index)
{
  const static char* TAG = "update_scheduler";
  bool day_sel = false;
  
  scheduler_t my_sche;
  my_sche.interval_time = interval;
  my_sche.start_time = start_1;
  my_sche.stop_time = stop_1;
  my_sche.index = index;

  if(strncmp(active_status,"True",4)==0 || strncmp(active_status,"true",4)==0)
    my_sche.active = true;
  else
    my_sche.active = false;
  
  if(strstr(day, "Monday")){
    app_set_scheduler(0, my_sche);
    ESP_LOGI(TAG, "Monday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Tuesday")){
    app_set_scheduler(1, my_sche);
    ESP_LOGI(TAG, "Tuesday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Wednesday")){
    app_set_scheduler(2, my_sche);
    ESP_LOGI(TAG, "Wednesday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Thursday")){
    app_set_scheduler(3, my_sche);
    ESP_LOGI(TAG, "Thursday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Friday")){
    app_set_scheduler(4, my_sche);
    ESP_LOGI(TAG, "Friday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Saturday")){
    app_set_scheduler(5, my_sche);
    ESP_LOGI(TAG, "Saturday Schedule updated!");
    day_sel = true;
  }
  if(strstr(day, "Sunday")){
    app_set_scheduler(6, my_sche);
    ESP_LOGI(TAG, "Sunday Schedule updated!");
    day_sel = true;
  }
  if(day_sel == false)
  {
    ESP_LOGI(TAG, "No day mentioned, nothing updated...");
    return;
  }
  
  return;
}

// handles websocket events
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;
  ESP_LOGI(TAG, "Socket_type: %d", type);

  switch(type) {
    
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG,"client %i connected!",num);
        if(current_state == GET_SCHEDULER){
            // /get/scheduler
            
            cJSON *fmt1;
            cJSON *data_scheduler = cJSON_CreateObject();
            cJSON *objects[APP_NUMBER_OF_DAY];
            bool addedSchedule = false;
            schedule_count = APP_NUMBER_OF_DAY;

            for(int i=0;i<APP_NUMBER_OF_DAY;i++) {
              objects[i] = cJSON_CreateObject();
            }
            cJSON *array;
            array = Create_array_of_anything(objects, APP_NUMBER_OF_DAY);
            uint8_t sche_index = 0;

            int day = 0;
            for (; day < APP_NUMBER_OF_DAY; day++)
            {
              uint8_t active = m_scheduler.day[day].num;
              ESP_LOGI(TAG, "Day is: %i \r\n schedule in plan : %i \r\n", day, active);
              
              for (int j = 0; j < active; j++)
              {
                start_1 = m_scheduler.day[day].scheduler[j].start_time;
                stop_1 = m_scheduler.day[day].scheduler[j].stop_time;
                interval = m_scheduler.day[day].scheduler[j].interval_time;
                uint8_t disp_ind = m_scheduler.day[day].scheduler[j].index;

                ESP_LOGI(TAG, "schedule : %i \r\n start : %i \r\n stop : %i \r\n interval : %i \r\n index : %i \r\n",j, start_1, stop_1, interval, disp_ind);
                
                for(int daySch=0; daySch < APP_NUMBER_OF_DAY; daySch++)
                {
                  if(cJSON_HasObjectItem(objects[daySch],"start") && cJSON_HasObjectItem(objects[daySch],"stop") && cJSON_HasObjectItem(objects[daySch],"interval"))
                  {
                    if((start_1 ==  cJSON_GetObjectItem(objects[daySch],"start")->valueint) && (stop_1 == cJSON_GetObjectItem(objects[daySch],"stop")->valueint)
                    && (interval == cJSON_GetObjectItem(objects[daySch],"interval")->valueint))
                    {
                        char newDat[100];
                        strcpy(newDat, cJSON_GetObjectItem(objects[daySch],"day")->valuestring);
                        strcat(newDat, ",");
                        strcat(newDat, days[day]);
                        cJSON_DeleteItemFromObject(objects[daySch],"day");
                        cJSON_AddStringToObject(objects[daySch],"day", newDat);
                        addedSchedule = true;
                        break;
                    }
                  }
                }
              
                if((addedSchedule == false)) {
                  cJSON_AddNumberToObject(objects[disp_ind],"start", start_1);
                  cJSON_AddNumberToObject(objects[disp_ind],"stop", stop_1);
                  cJSON_AddNumberToObject(objects[disp_ind],"interval", interval);
                  cJSON_AddStringToObject(objects[disp_ind],"day", days[day]);
                  ESP_LOGI(TAG,"Inside addedSchedule = false");
                  //sche_index++;
                }
                addedSchedule = false;
              }
            }

            for(int i=APP_NUMBER_OF_DAY-1; i >= 0; i--){
              if(cJSON_HasObjectItem(objects[i],"interval")==0){
                cJSON_DeleteItemFromArray(array,i);
                ESP_LOGI(TAG,"Deleting Object...%d\n", i);
                schedule_count--;
              }
            }
            cJSON_AddNumberToObject(data_scheduler, "scheduler_size", schedule_count);
            cJSON_AddItemToObject(data_scheduler, "scheduler", array);
            const char *my_json_string = cJSON_Print(data_scheduler);
            ESP_LOGI(TAG, "my_json_string\n%s",my_json_string);
            ESP_LOGI(TAG, "size: %d",strlen(my_json_string));  

            int status = ws_server_send_text_all_from_callback(my_json_string, strlen(my_json_string));
            if(status ==1)
                ESP_LOGI(TAG, "Message sent sucessfully\n");
            current_state = NONE;    
            cJSON_Delete(data_scheduler);
        }
      dat_req_rec = true;
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG,"client %i sent a disconnect message",num);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG,"client %i was disconnected",num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
      break;
    case WEBSOCKET_TEXT:
      if(len) 
      { 
        // if the message length was greater than zer 
        ESP_LOGI(TAG,"client %i sent message of size %i:\n%s",num,(uint32_t)len,msg);
        cJSON *root2 = cJSON_Parse(msg);

        if (root2 == NULL)
        {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                fprintf(stderr, "Error before: %s\n", error_ptr);

                cJSON* error_message = cJSON_CreateObject();
                cJSON_AddItemToObject(error_message,"request",cJSON_CreateString("get"));
                cJSON_AddItemToObject(error_message,"info",cJSON_CreateString("error"));
                cJSON_AddItemToObject(error_message,"msg",cJSON_CreateString("error in json parsing"));

                char* err_msg = cJSON_Print(error_message);

                ESP_LOGI(TAG,"%s",err_msg);
                int status_msg = ws_server_send_text_all_from_callback(err_msg,strlen(err_msg));
                if(status_msg ==1)
                  ESP_LOGI(TAG, "Message sent sucessfully\n");   
                cJSON_Delete(error_message);

            }
            // status = 0;
            return;
        }
        ESP_LOGI(TAG,"Message received, state : %d", current_state);


       // printf("%s   %s\n",req_info,req_info );

        // if(cJSON_HasObjectItem(root2, "current_epoch")){
        //   current_state = SET_CONFIG;
        //   printf("inside the function****************\n");
        // }

               
        if(cJSON_HasObjectItem(root2,"type") && !strcmp(cJSON_GetObjectItem(root2,"type")->valuestring,"check"))
        {
          
            cJSON* check_msg = cJSON_CreateObject();
            cJSON_AddItemToObject(check_msg,"type",cJSON_CreateString("check"));

            char * check_response  = cJSON_Print(check_msg);
            int status_msg = ws_server_send_text_client_from_callback(num,check_response, strlen(check_response));
            if(status_msg ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");   
            cJSON_Delete(check_msg);  
            break;

        }

        if(cJSON_HasObjectItem(root2, "current_epoch") || cJSON_HasObjectItem(root2, "sleep_mode") || cJSON_HasObjectItem(root2, "demo")){

         // printf("inside function 2***************\n");
            
            ESP_LOGI(TAG, "SET_CONFIG Websocket");          
            
            bool state = cJSON_HasObjectItem(root2, "current_epoch");

            if(state == 1)
            {     

                init_time = cJSON_GetObjectItem(root2,"current_epoch")->valueint;
                char *ptr_1 = cJSON_GetObjectItem(root2,"timeZone")->valuestring;
                memcpy(time_zone, ptr_1, strlen(ptr_1));
                removeSpaces(time_zone);
                removeSpaces(ptr_1);
                memcpy(print_time_zone, ptr_1,strlen(ptr_1));

                // char utc_str[] = "GMT";
                // for(int i=0; i<3; i++){
                //   time_zone[i] = utc_str[i];
                // }

                  if(time_zone[3] == '+')
                    time_zone[3] = '-';
                  else
                    time_zone[3] = '+';

                ESP_LOGI(TAG, "epoch received is %lu", init_time);

                ESP_LOGI(TAG, "timeZone received is %s", ptr_1);

                xStart=xStop; /// reset the counter

                xEventGroupSetBits(event_group, SET_TIME_STATE);

                cJSON_Delete(root2);
                current_state = NONE;
                dat_req_rec = true;
                start_in = false;
                store_gmt();
                ESP_LOGI(TAG, "<<<<<<<<<READING FROM FILE>>>>>>>>>>");
                retrive_gmt();
                break;
            } 

            const char *json_reqField = cJSON_GetObjectItem(root2,"request")->valuestring;
            state = cJSON_HasObjectItem(root2, "sleep_mode");
            if(state == 1){
                int mode = cJSON_GetObjectItem(root2,"sleep_mode")->valueint;
                ESP_LOGI(TAG, "Sleep mode : %d", mode);
                backup_scheduler_now();
                xEventGroupSetBits(event_group, PROCEED_STATE);
            }else{    
                int demo = cJSON_GetObjectItem(root2,"demo")->valueint;
                ESP_LOGI(TAG, "demo is : %d", demo);
                xEventGroupSetBits(event_group, DEMO_MODE_START);
                xEventGroupWaitBits(event_group, DEMO_MODE_STOP, pdTRUE, pdTRUE, portMAX_DELAY);
                ESP_LOGI(TAG, "-------- Demo Mode Completed! -----------");
            }
            
            cJSON_Delete(root2);
            current_state = NONE;
            dat_req_rec = true;
            break;
        }


        char* req_type = cJSON_GetObjectItem(root2,"request")->valuestring;
        
        char* req_info = cJSON_GetObjectItem(root2,"info")->valuestring;

        if(strncmp(req_info, "schedule",8)==0 && strncmp(req_type, "set",3)==0)
        {   
            ESP_LOGI(TAG, "SCHEDULER Handler Websocket");
            //ESP_LOGI(TAG, "root2->type=%s", JSON_Types(root2->type));
            cJSON *records = cJSON_GetObjectItem(root2,"scheduler");
            //ESP_LOGI(TAG, "records->type=%s", JSON_Types(records->type));
            int records_array_size = cJSON_GetArraySize(records);
            num_of_schedule = records_array_size;

            ESP_LOGI(TAG, "records_array_size = %d", records_array_size);
            for (int i=0;i<records_array_size;i++) {
                app_remove_scheduler_pass(i);
                cJSON *array = cJSON_GetArrayItem(records,i);
                start_1 = cJSON_GetObjectItem(array,"start")->valueint;
                stop_1 = cJSON_GetObjectItem(array,"stop")->valueint;
                interval = cJSON_GetObjectItem(array,"interval")->valueint;
                day = cJSON_GetObjectItem(array,"day")->valuestring;
                active_status = cJSON_GetObjectItem(array,"active")->valuestring;
                printf("*****************Day********* %s\n",day );
                ESP_LOGI(TAG, "start-%i = %i",i, cJSON_GetObjectItem(array,"start")->valueint);
                ESP_LOGI(TAG, "stop-%i = %i",i, cJSON_GetObjectItem(array, "stop")->valueint);
                ESP_LOGI(TAG, "interval-%i = %i",i, cJSON_GetObjectItem(array,"interval")->valueint);
                ESP_LOGI(TAG, "day-%i = %s",i, cJSON_GetObjectItem(array,"day")->valuestring);
                ESP_LOGI(TAG, "active = %s", cJSON_GetObjectItem(array,"active")->valuestring);
                update_scheduler(i);
            }
            
            cJSON_Delete(root2);

            current_state = NONE;
            dat_req_rec = true;
            xStart=xStop;
            // vTaskDelay(2000/portTICK_RATE_MS);

            uint32_t forecast_spray=0;
            backup_scheduler_now();
            for(int i=0;i<APP_NUMBER_OF_DAY;i++)
            {
              if(m_scheduler.day[i].num)
              {
            
                forecast_spray+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  
              }


            }

            ESP_LOGI(TAG," ***forecast_spray***%u",forecast_spray);


            bool read_s = nvs_read_data("spray_counter");

            if(read_s == 0){

              initialize_spary = run_time[0];

            } 
            else{

              initialize_spary = 3200;
              nvs_write_value("spray_counter",initialize_spary, 0);
              ESP_LOGI(TAG," spray count written to nvs memory.... :"); 
            }


            uint32_t days_left = 0;
            if(forecast_spray != 0){
          //initialize_spary = 100;
              ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
              ESP_LOGI(TAG,"Forecasted sprays : %d",forecast_spray); 
              ESP_LOGI(TAG,"Count of weeks for which the sprays will run %f",((float)initialize_spary/forecast_spray));
              days_left = ((float)initialize_spary / forecast_spray)*7;
                //days_left = days_left*24*60*60;
              ESP_LOGI(TAG,"days left : %d",days_left);
            }
            
            cJSON* update_days_left = cJSON_CreateObject();
            cJSON* request = cJSON_CreateString("get");
            cJSON* info = cJSON_CreateString("days_left");
            cJSON* rem_days = cJSON_CreateNumber(days_left);

            cJSON_AddItemToObject(update_days_left,"request",request);
            cJSON_AddItemToObject(update_days_left,"info",info);
            cJSON_AddItemToObject(update_days_left,"value",rem_days);

            //#here

            char * day_count  = cJSON_Print(update_days_left);
            ESP_LOGI(TAG,"\n%s\n",day_count);
            int status_msg = ws_server_send_text_client_from_callback(num,day_count, strlen(day_count));
            if(status_msg ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");   
            cJSON_Delete(update_days_left);

            ESP_LOGI("WEBSOCKET","Forecasted spray use per week : %u",forecast_spray);
            ESP_LOGI("WEBSOCKET","Forecasted spray use per month: %u",4*forecast_spray);

             
            break;            
            
        }

        else if(strncmp(req_info, "scheduler",9)==0 && strncmp(req_type, "get",3)==0)
        {
            cJSON *fmt1;
            cJSON *data_scheduler = cJSON_CreateObject();
            cJSON *objects[APP_NUMBER_OF_DAY];
            bool addedSchedule = false;
            schedule_count = APP_NUMBER_OF_DAY;

            for(int i=0;i<APP_NUMBER_OF_DAY;i++) {
              objects[i] = cJSON_CreateObject();
            }
            cJSON *array;
            array = Create_array_of_anything(objects, APP_NUMBER_OF_DAY);
            uint8_t sche_index = 0;

            int day = 0;
            for (; day < APP_NUMBER_OF_DAY; day++)
            {
              uint8_t active = m_scheduler.day[day].num;
              ESP_LOGI(TAG, "Day is: %i \r\n schedule in plan : %i \r\n", day, active);
              
              for (int j = 0; j < active; j++)
              {
                start_1 = m_scheduler.day[day].scheduler[j].start_time;
                stop_1 = m_scheduler.day[day].scheduler[j].stop_time;
                interval = m_scheduler.day[day].scheduler[j].interval_time;
                uint8_t disp_ind = m_scheduler.day[day].scheduler[j].index;
                bool active = m_scheduler.day[day].scheduler[j].active;

                ESP_LOGI(TAG, "schedule : %i \r\n start : %i \r\n stop : %i \r\n interval : %i \r\n index : %i \r\n",j, start_1, stop_1, interval, disp_ind);
                
                for(int daySch=0; daySch < APP_NUMBER_OF_DAY; daySch++)
                {
                  if(cJSON_HasObjectItem(objects[daySch],"start") && cJSON_HasObjectItem(objects[daySch],"stop") && cJSON_HasObjectItem(objects[daySch],"interval"))
                  {
                    if((start_1 ==  cJSON_GetObjectItem(objects[daySch],"start")->valueint) && (stop_1 == cJSON_GetObjectItem(objects[daySch],"stop")->valueint)
                    && (interval == cJSON_GetObjectItem(objects[daySch],"interval")->valueint))
                    {
                        char newDat[100];
                        strcpy(newDat, cJSON_GetObjectItem(objects[daySch],"day")->valuestring);
                        strcat(newDat, ",");
                        strcat(newDat, days[day]);
                        cJSON_DeleteItemFromObject(objects[daySch],"day");
                        cJSON_AddStringToObject(objects[daySch],"day", newDat);
                        addedSchedule = true;
                        break;
                    }
                  }
                }
              
                if((addedSchedule == false)) {

                  cJSON_AddNumberToObject(objects[disp_ind],"start", start_1);
                  cJSON_AddNumberToObject(objects[disp_ind],"stop", stop_1);
                  cJSON_AddNumberToObject(objects[disp_ind],"interval", interval);
                  cJSON_AddStringToObject(objects[disp_ind],"day", days[day]);
                  cJSON_AddNumberToObject(objects[disp_ind],"index",disp_ind+1);
                 
                 if(active)
                    cJSON_AddStringToObject(objects[disp_ind],"active", "true");
                  else
                    cJSON_AddStringToObject(objects[disp_ind],"active", "false");

                  //sche_index++;
                }
                addedSchedule = false;
              }
            }

            for(int i=APP_NUMBER_OF_DAY-1; i >= 0; i--){
              if(cJSON_HasObjectItem(objects[i],"interval")==0){
                cJSON_DeleteItemFromArray(array,i);
                ESP_LOGI(TAG,"Deleting Object...%d\n", i);
                schedule_count--;
              }
            }
            cJSON_AddNumberToObject(data_scheduler, "scheduler_size", schedule_count);
            
            cJSON_AddItemToObject(data_scheduler, "scheduler", array);
            const char *my_json_string = cJSON_Print(data_scheduler);
            ESP_LOGI(TAG, "my_json_string\n%s",my_json_string);
            ESP_LOGI(TAG, "size: %d",strlen(my_json_string));  

            int status = ws_server_send_text_all_from_callback(my_json_string, strlen(my_json_string));
            if(status ==1)
                ESP_LOGI(TAG, "Message sent sucessfully\n");
            current_state = NONE;    
            cJSON_Delete(data_scheduler);


            //dat_req_rec = true;
            xStart=xStop;
            
            break;

        }

        else if(strncmp(req_info, "scheduler",9)==0 && strncmp(req_type, "status",6)==0)
        {

            int index = cJSON_GetObjectItem(root2,"index")->valueint;
            const char* value = cJSON_GetObjectItem(root2,"value")->valuestring;
             ESP_LOGI(TAG, "index passed is : %d", index);

            bool status;
            
            if(strncmp(value,"true",4)==0)
            {
              ESP_LOGI(TAG,"setting index %d schedules active\n",index);
                status = true;
                
            }

            else if(strncmp(value,"false",5)==0)
            {
              ESP_LOGI(TAG,"setting index %d schedules unactive\n",index);
                status = false;
            }


            for (int i = 0; i < APP_NUMBER_OF_DAY; i++)
              {
                uint8_t active = m_scheduler.day[i].num;
                for (int j = 0; j < active; j++)
                {
                  if(m_scheduler.day[i].scheduler[j].index == index-1)
                  {

                      m_scheduler.day[i].scheduler[j].active = status;                    
                  }
                }
              }

            //int val = atoi(index_del);
           

          //   cJSON* active_index_obj=NULL;
          //   active_index_obj = cJSON_CreateObject();

          //   cJSON_AddNumberToObject(active_index_obj,"index",index);

          //   cJSON_AddStringToObject(active_index_obj,"info","schduler");

          //   if(index==active_i-1)
          //     cJSON_AddStringToObject(active_index_obj,"active","true");
          //   else
          //     cJSON_AddStringToObject(active_index_obj,"active","false");

          //   char* check_active_response = cJSON_Print(active_index_obj);

          //   ESP_LOGI(TAG,"\n%s\n",check_active_response);
          //   int status_w = ws_server_send_text_client_from_callback(num,check_active_response, strlen(check_active_response));
          // if(status_w ==1)
          //   ESP_LOGI(TAG, "Message sent sucessfully\n");

          // cJSON_Delete(active_index_obj);
              xStart=xStop;
             // backup_scheduler_now();
              //#nowhere

            uint32_t forecast_spray=0;
            for(int i=0;i<APP_NUMBER_OF_DAY;i++)
            {
              if(m_scheduler.day[i].num)
              {
            
                forecast_spray+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  
              }


            }

            ESP_LOGI(TAG,"%u",forecast_spray);



            bool read_s = nvs_read_data("spray_counter");

            if(read_s == 0){

              initialize_spary = run_time[0];

            } 
            else{

              initialize_spary = 3200;
            }


            uint32_t days_left = 0;
            if(forecast_spray != 0){
          //initialize_spary = 100;
              ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
              days_left = ((float)initialize_spary / forecast_spray)*7;
                //days_left = days_left*24*60*60;
              ESP_LOGI(TAG,"days left : %d",days_left);
            }
            
            cJSON* update_days_left = cJSON_CreateObject();
            cJSON* request = cJSON_CreateString("get");
            cJSON* info = cJSON_CreateString("days_left");
            cJSON* rem_days = cJSON_CreateNumber(days_left);

            cJSON_AddItemToObject(update_days_left,"request",request);
            cJSON_AddItemToObject(update_days_left,"info",info);
            cJSON_AddItemToObject(update_days_left,"value",rem_days);

            //#here

            char * day_count  = cJSON_Print(update_days_left);
            ESP_LOGI(TAG,"\n%s\n",day_count);
            int status_msg = ws_server_send_text_client_from_callback(num,day_count, strlen(day_count));
            if(status_msg ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");   
            cJSON_Delete(update_days_left);
            backup_scheduler_now();
             
          break;
  
          
        }

        else if( strncmp(req_info, "type_disp", 9) == 0 || cJSON_HasObjectItem(root2,"value")){
        ESP_LOGI(TAG, "SET_INFO Websocket");
        if(strncmp(req_type, "init", 4) == 0)
        { 
            ESP_LOGI(TAG,"Initializing Spray count");
            const char *json_reqField = cJSON_GetObjectItem(root2,"info")->valuestring;
            char *spray_init = cJSON_GetObjectItem(root2,"value")->valuestring;       
            initialize_spary = strtol(spray_init, &spray_init, 10);
            ESP_LOGI(TAG, "Spray initilization received : %d", initialize_spary);
            nvs_write_value("Spray_info",initialize_spary, 0);

            //calculate days_left after initializing the spray count

            uint32_t new_count=0;
            for(int i=0;i<APP_NUMBER_OF_DAY;i++)
            {

              if(m_scheduler.day[i].num)
              {
                new_count+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  

                ESP_LOGI(TAG,"##########Calculated days_left......########## %d",new_count);

              }


            }



            bool read_new_count = nvs_read_data("Spray_info");

            if(read_new_count == 0){

              initialize_spary = run_time[0];
            } 
            else{

              initialize_spary = 3200;
            }



            // send the updated days_left count 
            remaining_sprays=initialize_spary;

            nvs_write_value("spray_counter",remaining_sprays, 0);



            ESP_LOGI(TAG,"Remaining Sprays set to %d",remaining_sprays);

            uint32_t days_left = 0;
            if(new_count != 0){
          //initialize_spary = 100;
              ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
              days_left = ((float)initialize_spary / new_count)*7;
                //days_left = days_left*24*60*60;
              ESP_LOGI(TAG,"days left : %d",days_left);
            }


            cJSON* main_object = cJSON_CreateObject();
            cJSON* main_array = cJSON_CreateArray();

            // Sprays left json object
            cJSON* update_days_left = cJSON_CreateObject();
            cJSON* request = cJSON_CreateString("get");
            cJSON* info = cJSON_CreateString("days_left");
            cJSON* rem_days = cJSON_CreateNumber(days_left);

            // Remaining sprays json object
            cJSON* update_rem_sprays = cJSON_CreateObject();
            cJSON* rem_spray_request=cJSON_CreateString("get");
            cJSON* rem_sprays_info = cJSON_CreateString("rem_sprays");
            cJSON* rem_sprays = cJSON_CreateNumber(remaining_sprays);


            cJSON* update_init_sprays = cJSON_CreateObject();
            cJSON* update_init1_sprays = cJSON_CreateObject();
            cJSON_AddItemToObject(update_init_sprays,"request",cJSON_CreateString("get"));
            cJSON_AddItemToObject(update_init_sprays,"info",cJSON_CreateString("spray"));
            cJSON_AddItemToObject(update_init_sprays,"value",cJSON_CreateNumber(initialize_spary));


            cJSON_AddItemToObject(update_init1_sprays,"request",cJSON_CreateString("init"));
            cJSON_AddItemToObject(update_init1_sprays,"info",cJSON_CreateString("spray"));
            cJSON_AddItemToObject(update_init1_sprays,"value",cJSON_CreateNumber(initialize_spary));


            cJSON_AddItemToObject(update_days_left,"request",request);
            cJSON_AddItemToObject(update_days_left,"info",info);
            cJSON_AddItemToObject(update_days_left,"value",rem_days);
            
            cJSON_AddItemToObject(update_rem_sprays,"request",rem_spray_request);
            cJSON_AddItemToObject(update_rem_sprays,"info",rem_sprays_info);
            cJSON_AddItemToObject(update_rem_sprays,"value",rem_sprays);

            cJSON_AddItemToArray(main_array,update_init_sprays);
            cJSON_AddItemToArray(main_array,update_init1_sprays);
            cJSON_AddItemToArray(main_array,update_days_left);
            cJSON_AddItemToArray(main_array,update_rem_sprays);

            cJSON_AddItemToObject(main_object,"snapshotinfo",main_array);

            //#here

            char * day_count  = cJSON_Print(main_object);
            ESP_LOGI(TAG,"\n%s\n",day_count);
            int status_msg = ws_server_send_text_client_from_callback(num,day_count, strlen(day_count));
            if(status_msg ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");   
            cJSON_Delete(main_object);



        }
        else if(strncmp(req_type, "set", 3) == 0)
        {    
            //const char *json_reqField = cJSON_GetObjectItem(root2,"Dispenser_type")->valuestring;
            memset(Disp_type, 0 , sizeof(Disp_type));
            memcpy(Disp_type, cJSON_GetObjectItem(root2,"Dispenser_type")->valuestring, strlen(cJSON_GetObjectItem(root2,"Dispenser_type")->valuestring));
            ESP_LOGI(TAG, "Dispenser type received : %s", Disp_type);
        }
        cJSON_Delete(root2);
        current_state = NONE;
        dat_req_rec = true;
        // return;
        break;
        }
      else if(strncmp(req_info, "snapshot",8)==0 && strncmp(req_type, "get",3)==0)
       {

        ESP_LOGI(TAG, "GET_SNAPSHOT Websocket"); 
        cJSON* snapObj=cJSON_CreateObject();

        cJSON* snapArr= cJSON_CreateArray();

        cJSON* arrItem = cJSON_CreateObject();

        cJSON* request = cJSON_CreateString("get");
        cJSON* info = cJSON_CreateString("battery");
        cJSON* value = cJSON_CreateNumber(battery_level);

        // printf("Battery Voltage: %d",get_battery());

        cJSON_AddItemToObject(arrItem, "request", request);
        cJSON_AddItemToObject(arrItem, "info", info);
        cJSON_AddItemToObject(arrItem, "value", value);

        cJSON_AddItemToArray(snapArr, arrItem);

        arrItem = cJSON_CreateObject();


        request = cJSON_CreateString("get");
        info = cJSON_CreateString("spray");

         //uint32_t s_info=0;
         bool spray_read = nvs_read_data("Spray_info");

         if(spray_read == 0){

                initialize_spary = run_time[0];

              } 
              else{

                initialize_spary = 3200;
              }

        value =cJSON_CreateNumber(initialize_spary);

        cJSON_AddItemToObject(arrItem, "request", request);
        cJSON_AddItemToObject(arrItem, "info", info);
        cJSON_AddItemToObject(arrItem, "value", value);
        cJSON_AddItemToArray(snapArr, arrItem);

        uint32_t spray_time=0;
        arrItem = cJSON_CreateObject();
        request = cJSON_CreateString("get");
        info = cJSON_CreateString("days_left");

        uint32_t i_spray;
        bool spray_status = nvs_read_data("spray_counter");

        if(spray_status == 0){

          i_spray = run_time[0];

        } 
        else{

          i_spray = 3200;
        }



        for(int i=0;i<APP_NUMBER_OF_DAY;i++)
        {
          if(m_scheduler.day[i].num)
          {

            spray_time+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  
          }


        }

        ESP_LOGI(TAG,"%u",spray_time);

        days_left = 0;
        if(spray_time != 0){
          //initialize_spary = 100;
          ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
          days_left = ((float)i_spray / spray_time)*7;
                //days_left = days_left*24*60*60;
          ESP_LOGI(TAG,"days left : %d",days_left);
        }

        value =cJSON_CreateNumber(days_left);
        cJSON_AddItemToObject(arrItem, "request", request);
        cJSON_AddItemToObject(arrItem, "info", info);
        cJSON_AddItemToObject(arrItem, "value", value);
        cJSON_AddItemToArray(snapArr, arrItem);

      
        arrItem = cJSON_CreateObject();
        request = cJSON_CreateString("get");
        info = cJSON_CreateString("rem_sprays"); 


        // uint32_t i_spray;
        //  bool spray_status = nvs_read_data("spray_counter");
             
        //       if(spray_status == 0){

        //         i_spray = run_time[0];

        //       } 
        //       else{

        //         i_spray = 3200;
        //       }

        value =cJSON_CreateNumber(i_spray);

        cJSON_AddItemToObject(arrItem, "request", request);
        cJSON_AddItemToObject(arrItem, "info", info);
        cJSON_AddItemToObject(arrItem, "value", value);
        cJSON_AddItemToArray(snapArr, arrItem);

        cJSON* versionInfo = cJSON_CreateObject();
        cJSON_AddItemToObject(versionInfo,"request",cJSON_CreateString("get"));
        cJSON_AddItemToObject(versionInfo,"info",cJSON_CreateString("version"));
        cJSON_AddItemToObject(versionInfo,"value",cJSON_CreateString(VERSION));
        cJSON_AddItemToArray(snapArr,versionInfo);

        // arrItem = cJSON_CreateObject();
        // request = cJSON_CreateString("get");
        // info = cJSON_CreateString("spray_counter");

        // uint32_t spray_c;
        // bool status = nvs_read_data("spray_counter");
        //     if(status == 0){
        //       spray_c = run_time[0];
        //     } else{
        //       spray_c = 37680;
        //     }
            
        // value =cJSON_CreateNumber(spray_c);

        // cJSON_AddItemToObject(arrItem, "request", request);
        // cJSON_AddItemToObject(arrItem, "info", info);
        // cJSON_AddItemToObject(arrItem, "value", value);
        // cJSON_AddItemToArray(snapArr, arrItem);

        cJSON_AddItemToObject(snapObj, "snapshotinfo", snapArr);

        char * snapString  = cJSON_Print(snapObj);
       ESP_LOGI(TAG,"\n%s\n",snapString);
       int status_w = ws_server_send_text_client_from_callback(num,snapString, strlen(snapString));
        if(status_w ==1)
          ESP_LOGI(TAG, "Message sent sucessfully\n");
        //current_state = NONE;    
        cJSON_Delete(snapObj);
        break;
    }



      else if(strncmp(req_info, "scheduler",9)==0 && strncmp(req_type, "delete",6)==0)
        {
          if(cJSON_HasObjectItem(root2,"deleteindex") == 1){
            char *index_del = cJSON_GetObjectItem(root2,"deleteindex")->valuestring;
            int val = atoi(index_del);
            ESP_LOGI(TAG, "index passed for deletion is : %d", val-1);

            if((val <= 5) && (val > 0))
            {
              ESP_LOGI(TAG, "Deleting index at %i \r\n",val);
              app_remove_scheduler_pass(val-1);
              xStart=xStop;
            }
            else
            {
                for(int i=0; i <= num_of_schedule; i++)
                {
                  ESP_LOGI(TAG, "Deleting index at %i \r\n total : %i \r\n",i, num_of_schedule);
                  app_remove_scheduler_pass(i);
                  
                }
                xStart=xStop;
            }


            uint32_t forecast_spray=0;
            backup_scheduler_now();

            for(int i=0;i<APP_NUMBER_OF_DAY;i++)
            {
              if(m_scheduler.day[i].num)
              {
            
                forecast_spray+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  
              }


            }

            ESP_LOGI(TAG,"%u",forecast_spray);


            bool read_s = nvs_read_data("spray_counter");

            if(read_s == 0){

              initialize_spary = run_time[0];

            } 
            else{

              initialize_spary = 3200;
            }


            uint32_t days_left = 0;

            if(forecast_spray != 0){
          //initialize_spary = 100;
              ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
              ESP_LOGI(TAG,"Forecasted sprays : %d",forecast_spray); 
              ESP_LOGI(TAG,"Count of weeks for which the sprays will run %f",((float)initialize_spary/forecast_spray));
              days_left = ((float)initialize_spary / forecast_spray)*7;
                //days_left = days_left*24*60*60;
              ESP_LOGI(TAG,"days left : %d",days_left);
            }
            
            cJSON* update_days_left = cJSON_CreateObject();
            cJSON* request = cJSON_CreateString("get");
            cJSON* info = cJSON_CreateString("days_left");
            cJSON* rem_days = cJSON_CreateNumber(days_left);

            cJSON_AddItemToObject(update_days_left,"request",request);
            cJSON_AddItemToObject(update_days_left,"info",info);
            cJSON_AddItemToObject(update_days_left,"value",rem_days);

            //#here

            char * day_count  = cJSON_Print(update_days_left);
            ESP_LOGI(TAG,"\n%s\n",day_count);
            int status_msg = ws_server_send_text_client_from_callback(num,day_count, strlen(day_count));
            if(status_msg ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");   
            cJSON_Delete(update_days_left);



          }
        }





      else if(cJSON_HasObjectItem(root2,"info") == 1){
            char *info_type = cJSON_GetObjectItem(root2,"info")->valuestring;
            char *info_req = cJSON_GetObjectItem(root2,"request")->valuestring;

          data_info = cJSON_CreateObject();
          if(strncmp(info_type, "battery",7) == 0){
              // // battery voltage
              int Vbat = get_battery();
              // // get/info   
              cJSON_AddStringToObject(data_info, "request", info_req);
              cJSON_AddStringToObject(data_info,"info", "battery");
              cJSON_AddNumberToObject(data_info,"value", 3.3);
          }
          else if(strncmp(info_type, "days_left",9) == 0){
              uint32_t spray_time = 0, exclude_time = 0;
              // for (int day=0; day < APP_NUMBER_OF_DAY; day++)
              // {
              //   uint8_t active = m_scheduler.day[day].num;
              //   for (int j = 0; j < active; j++)
              //   {
              //     uint32_t init = m_scheduler.day[day].scheduler[j].start_time/3600;
              //     uint32_t end = m_scheduler.day[day].scheduler[j].stop_time/3600;
              //     if(m_scheduler.day[day].scheduler[j].interval_time != 0)
              //     {
              //       spray_time += ((end - init)*60)/m_scheduler.day[day].scheduler[j].interval_time;
              //       ESP_LOGI(TAG,"spray_time : %d",spray_time);
              //     }
              //     else
              //     {
              //       exclude_time += (end - init)*60;
              //     }
              //   }
              // }


            for(int i=0;i<APP_NUMBER_OF_DAY;i++)
            {
              if(m_scheduler.day[i].num)
              {
            
                spray_time+= count_spray(m_scheduler.day[i].scheduler,m_scheduler.day[i].num);  
              }


            }

            ESP_LOGI(TAG,"%u",spray_time);

              days_left = 0;
              //spray_time -= exclude_time;
              if(spray_time != 0){
                initialize_spary = 100;
                ESP_LOGI(TAG,"initialize_spary : %d",initialize_spary);
                days_left = ((float)initialize_spary / spray_time)*7;
                //days_left = days_left*24*60*60;
                ESP_LOGI(TAG,"days left : %d",days_left);
              }
              cJSON_AddStringToObject(data_info, "request", info_req);
              cJSON_AddStringToObject(data_info,"info", "days_left");
              cJSON_AddNumberToObject(data_info,"value", days_left);
          }

          else if(strncmp(info_type, "spray_counter",13) == 0)
          {
            uint32_t spray_counter;
            bool status = nvs_read_data("spray_counter");
            if(status == 0){
              spray_counter = run_time[0];
            } else{
              spray_counter = 37680;
              ESP_LOGI(TAG,"spray_counter info lost from NVS\n");
              ESP_LOGI(TAG,"spray_counter re-init to 37680\n");
              nvs_write_value("spray_counter",spray_counter, 0);
            }
            
            cJSON_AddStringToObject(data_info, "request", "get");
            cJSON_AddStringToObject(data_info,"info", "spray_counter");
            cJSON_AddNumberToObject(data_info,"value", spray_counter);
          }

          else
          {
              bool status = nvs_read_data("Spray_info");
              if(status == 0){
                initialize_spary = run_time[0];
              } else{
                initialize_spary = 3200;
                ESP_LOGI(TAG,"Spray info lost from NVS\n");
                ESP_LOGI(TAG,"Spray re-init to 3200\n");
                nvs_write_value("Spray_info",initialize_spary, 0);
              }
              cJSON_AddStringToObject(data_info, "request", info_req);
              cJSON_AddStringToObject(data_info,"info", "spray");
              cJSON_AddNumberToObject(data_info,"value", initialize_spary);
          }
          // log json data
          ESP_LOGI(TAG, "Replying to client %i",num);
          const char *my_json_string = cJSON_Print(data_info);
          ESP_LOGI(TAG, "my_json_string\n%s",my_json_string);
          ESP_LOGI(TAG, "size : %d",strlen(my_json_string));
          
          int status = ws_server_send_text_all_from_callback(my_json_string, strlen(my_json_string));
          if(status ==1)
              ESP_LOGI(TAG, "Message sent sucessfully\n");
              
          cJSON_Delete(root2);
          cJSON_Delete(data_info);
          dat_req_rec = true;
        }
      }
      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG,"client %i responded to the ping",num);
      break;
  }
  return;
}

// serves any clients
static void http_serve(struct netconn *conn) {
  const static char* TAG = "http_server";
  //const static char JSON_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/json\n\n";
  num_clinet = 1;

  xStart = xTaskGetTickCount();
  struct netbuf* inbuf;
  static char* buf;
  static uint16_t buflen;
  static err_t err;

  netconn_set_recvtimeout(conn,10000); // allow a connection timeout of 1 second
  ESP_LOGI(TAG,"reading from client...");
  err = netconn_recv(conn, &inbuf);
  if(err==ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    ESP_LOGI(TAG,"read from client %s", buf);
    if(buf) {

     // websocket
      if(strstr(buf,"GET /")
          && strstr(buf,"Upgrade: websocket") && (start_in)) {
        ESP_LOGI(TAG,"Sending ...");
        // ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
        //netbuf_delete(inbuf);    
      }
            
      if(strstr(buf,"GET /set/config ")
          && strstr(buf,"Upgrade: websocket")) {
              
        ESP_LOGI(TAG,"Sending /set/config");
        current_state = SET_CONFIG;
      }

      if(strstr(buf,"GET /")
          && strstr(buf,"Upgrade: websocket") && (start_in)) {
        ESP_LOGI(TAG,"Sending first thing");
       // ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
        //netbuf_delete(inbuf);    
      }
      else if(strstr(buf,"GET /get/info ")
          && strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Sending /get/info");
        current_state = OTHER_STATE;
      }

      else if(strstr(buf,"GET /get/scheduler ")
          && strstr(buf,"Upgrade: websocket")) {
        ESP_LOGI(TAG,"Sending /get/scheduler");
        
        current_state = GET_SCHEDULER;
      }
      else if(strstr(buf,"GET /set/scheduler ")
          && strstr(buf,"Upgrade: websocket")) {
              
        ESP_LOGI(TAG,"Sending /set/scheduler");
        current_state = SET_SCHEDULER;
      }
      else if(strstr(buf,"GET /set/info ")
          && strstr(buf,"Upgrade: websocket")) {
              
        ESP_LOGI(TAG,"Sending /set/info");
        current_state = SET_INFO;
      }
      else if(strstr(buf,"GET /set/deleteschedule ")
          && strstr(buf,"Upgrade: websocket")) {
              
        ESP_LOGI(TAG,"Sending /set/deleteschedule");
        current_state = DEL_SCHEDULE;
      }
      else if(strstr(buf,"GET /get/snapshot")
          && strstr(buf,"Upgrade: websocket")) {
              
        ESP_LOGI(TAG,"Sending /get/snapshot");
        current_state = GET_SNAPSHOT;
      }

      else {
        ESP_LOGI(TAG,"Unknown request");
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }
    }
    else {
      ESP_LOGI(TAG,"Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  }
  else { // if err==ERR_OK
    ESP_LOGI(TAG,"error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
  
  ws_server_add_client(conn,buf,buflen,"/",websocket_callback); 
  while(dat_req_rec == false){
      vTaskDelay(1/portTICK_RATE_MS);
  }
  while(dat_req_rec == false);
  dat_req_rec = false;  
  netbuf_delete(inbuf);    
}

// handles clients when they first connect. passes to a queue
static void server_task(void* pvParameters) {
  const static char* TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  client_queue = xQueueCreate(client_queue_size,sizeof(struct netconn*));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn,NULL,MY_PORT);
  netconn_listen(conn);
  ESP_LOGI(TAG,"server listening");
  do {
    err = netconn_accept(conn, &newconn);
    if(err == ERR_OK) {
      ESP_LOGI(TAG,"new client");
      xQueueSendToBack(client_queue,&newconn,portMAX_DELAY);
      //http_serve(newconn);
    }  
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG,"task ending, rebooting board");
  esp_restart();
}

// receives clients from queue, handles them
static void server_handle_task(void* pvParameters) {
  const static char* TAG = "server_handle_task";
  struct netconn* conn, prev_conn;
  ESP_LOGI(TAG,"task starting");
  for(;;) {
    xQueueReceive(client_queue,&conn,portMAX_DELAY);
    ESP_LOGI(TAG,"NewClient Queue received..");

    if(!conn) continue;
    //ws_server_add_client(conn,NULL,0,"/",websocket_callback);
    http_serve(conn);
  }
  vTaskDelete(NULL);
}

static void count_task(void* pvParameters) {
  const static char* TAG = "count_task";
  char out[20];
  int len;
  int clients;
  const static char* word = "%i";
  uint8_t n = 0;
  const int DELAY = 1000 / portTICK_PERIOD_MS; // 1 second
  uint32_t time_elapsed = 0;

  xStart = xTaskGetTickCount();
  ESP_LOGI(TAG,"starting task");
  for(;;) {
    len = sprintf(out,word,n);
    //clients = ws_server_send_text_all(out,len);
    if(clients > 0) {
      //ESP_LOGI(TAG,"sent: \"%s\" to %i clients",out,clients);
    }
    n++;
    vTaskDelay(DELAY);
    xStop = xTaskGetTickCount();
    time_elapsed = xStop - xStart;
    ESP_LOGI(TAG,"elapsed time : %i secs", time_elapsed/100);

    // wait for 35secs if no activity over WebSocket proceed for normal program working, 120 sec or 2 mins
    if(time_elapsed > WAIT_TIME_APP){
      // generate an event flag to fire main stream ahead
      backup_scheduler_now();
      xEventGroupSetBits(event_group, PROCEED_STATE);
    }
  }
}

void start_websocket_server() {
  ws_server_start();
  xTaskCreate(&server_task,"server_task",3000,NULL,9,NULL);
  xTaskCreate(&server_handle_task,"server_handle_task",4000,NULL,6,NULL);
  xTaskCreate(&count_task,"count_task",6000,NULL,2,NULL);
}
