#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "osal/times.h"
#include "osal/task.h"
#include "config/config.h"
#include "esp_sleep.h"
#include<freertos/list.h>
#include <sys/time.h>
#include "sdk/wifi.h"
#include "driver/rtc_io.h"
#include<math.h>
#define TAG "APP"
#define MOTOR_OUT     (35)
#define APP_IS_SET    (0xAB)
#define APP_SET_TAG                   "APPSET"
#define APP_DAY_PREFIX                "APPDAY"
#define APP_DAY_NUMBER_PREFIX         "APPDAYNUM"
#define APP_SCHEDUER_START_PREFIX     "APPSCHESTART"
#define APP_SCHEDUER_STOP_PREFIX      "APPSCHESTOP"
#define APP_SCHEDUER_INTERVAL_PREFIX  "APPSCHEINTER"
#define APP_S_TO_MS_FACTOR 			   1000ULL
#define DEBUG_MODE
#define ZERO   0

static uint32_t tokenize_string(char *str);
RTC_DATA_ATTR char time_zone[50];
RTC_DATA_ATTR char print_time_zone[50];
extern uint32_t* run_time;


bool nvs_write_value(char *nvs_area,uint32_t nvs_value, int index);
extern void close_nvs(void);

RTC_DATA_ATTR app_scheduler_t m_scheduler;
//RTC_DATA_ATTR scheduler_t overlapping_schedule;
RTC_DATA_ATTR scheduler_t current_scheduler;
//RTC_DATA_ATTR bool woke_up_first_time=true;
//RTC_DATA_ATTR bool switch_back = false;
//RTC_DATA_ATTR bool is_current_schedule_stop = false;

// RTC_DATA_ATTR scheduler_t app_today_schedules[APP_SUPPORT_SIZE];
// RTC_DATA_ATTR uint16_t index_of_overlapped_schedule[APP_SUPPORT_SIZE];
// RTC_DATA_ATTR uint16_t index_of_current_schedule;
// RTC_DATA_ATTR uint16_t count_of_overlapped_schedules;
//RTC_DATA_ATTR bool overlapping_start=true;
//RTC_DATA_ATTR bool overlapping_stop=true;

// RTC_DATA_ATTR uint32_t overlapping_starttime;
// RTC_DATA_ATTR uint32_t overlapping_stoptime;
// RTC_DATA_ATTR uint8_t app_today_active;
RTC_DATA_ATTR uint8_t best_fit_scheduler_index[APP_SUPPORT_SIZE];
RTC_DATA_ATTR uint32_t spray_count;
RTC_DATA_ATTR uint32_t remaining_sprays;

uint32_t old_millis = 0;


char tag[32] = {'\0'};
int schedule_day = 0;




char *schedule[APP_NUMBER_OF_DAY] = {"schedule_1", "schedule_2", "schedule_3", "schedule_4", "schedule_5", "schedule_6", "schedule_7"};
//char *num[]= {"num_1", "num_2", "num_3", "num_4", "num_5", "num_6", "num_7"};
//char *start[] = {"start_1", "start_2", "start_3", "start_4", "start_5", "start_6", "start_7"};
//char *start[] = {"stop_1", "stop_2", "stop_3", "stop_4", "stop_5", "stop_6", "stop_7"};
//char *interval[] = {"interval_1", "interval_2", "interval_3", "interval_4", "interval_5", "interval_6", "interval_7"};

RTC_DATA_ATTR uint8_t app_today = 0; //Today - will be set at app_init
RTC_DATA_ATTR sleep_schedule_t m_sleep_schedule;
extern void erase_nvs_now(void);

void store_gmt(){
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen("/spiffs/gmt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, time_zone);
    fclose(f);
    ESP_LOGI(TAG, "File written");
    fclose(f);
}

void retrive_gmt(){
	ESP_LOGI(TAG, "Reading file gmt");
    FILE* f = fopen("/spiffs/gmt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    line[14] = '\0';
    ESP_LOGI(TAG, "Read from file: '%s'", line);
    memcpy(time_zone, line, 50);
    fclose(f);
}

unsigned long millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}


void app_set_old_millis(){
	old_millis = millis();
	ESP_LOGI(TAG, "****************old_millis***************** '%d'", old_millis);
}

// uint32_t get_old_millis(){

// }

static char* app_get_tag(char* prefix, uint8_t day, uint8_t index)
{
	memset(tag, '\0', 32);
	int len = sprintf(tag, "%s%d%d", prefix, day, index);
	tag[len] = '\0';
	return tag;
}


void swap(scheduler_t *xp, scheduler_t *yp)
{
    scheduler_t temp = *xp;
    *xp = *yp;
    *yp = temp;
}


void sort_schedule(scheduler_t* schedule_array,int size)
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
       		if (schedule_array[j].stop_time > schedule_array[j+1].stop_time)
            	swap(&schedule_array[j], &schedule_array[j+1]);

       	} 

    }

	}

}


void app_init(void)
{
	gpio_set_direction(MOTOR_OUT, GPIO_MODE_OUTPUT);
	ESP_LOGI(TAG, "Motor OFF");

	//Init external pins
	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(BUTTON_WAKEUP_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_WAKEUP_GPIO, GPIO_PULLUP_ONLY);
	//	gpio_set_level(LED_GPIO, 1);

	// uint32_t app_status = 0x00;
	// uint32_t value = 0x00;

	// if (ESP_OK == config_get_u32(APP_SET_TAG, &app_status))
	// {
	// 	if (app_status == APP_IS_SET)
	// 	{
	// 		for (int i = 0; i < APP_NUMBER_OF_DAY; i++)
	// 		{
	// 			config_get_u32(app_get_tag(APP_DAY_NUMBER_PREFIX, i, 0), &value);
	// 			m_scheduler.day[i].num = value;
	// 			for (int j = 0; j < APP_SUPPORT_SIZE; j++)
	// 			{
	// 				config_get_u32(app_get_tag(APP_SCHEDUER_START_PREFIX, i, j), &value);
	// 				m_scheduler.day[i].scheduler[j].start_time = value;
	// 				config_get_u32(app_get_tag(APP_SCHEDUER_STOP_PREFIX, i, j), &value);
	// 				m_scheduler.day[i].scheduler[j].stop_time = value;
	// 				config_get_u32(app_get_tag(APP_SCHEDUER_INTERVAL_PREFIX, i, j), &value);
	// 				m_scheduler.day[i].scheduler[j].interval_time = value;
	// 			}
	// 		}
	// 		return;
	// 	}
	// }

	// config_set_u32(APP_SET_TAG, APP_IS_SET);
	// for (int i = 0; i < APP_NUMBER_OF_DAY; i++)
	// {
	// 	config_set_u32(app_get_tag(APP_DAY_NUMBER_PREFIX, i, 0), 0);
	// 	m_scheduler.day[i].num = value;
	// 	for (int j = 0; j < APP_SUPPORT_SIZE; j++)
	// 	{
	// 		config_set_u32(app_get_tag(APP_SCHEDUER_START_PREFIX, i, j), 0);
	// 		m_scheduler.day[i].scheduler[j].start_time = 0;
	// 		config_set_u32(app_get_tag(APP_SCHEDUER_STOP_PREFIX, i, j), 0);
	// 		m_scheduler.day[i].scheduler[j].stop_time = 0;
	// 		config_set_u32(app_get_tag(APP_SCHEDUER_INTERVAL_PREFIX, i, j), 0);
	// 		m_scheduler.day[i].scheduler[j].interval_time = 0;
	// 	}
	// }
}


void app_set_scheduler(int day, scheduler_t scheduler)
{
	uint8_t index_scheduler = m_scheduler.day[day].num;
	scheduler_t compare_scheduler;

	for (int j = 0; j < index_scheduler; j++)
	{
		compare_scheduler = m_scheduler.day[day].scheduler[j];
		if((scheduler.start_time == compare_scheduler.start_time) && (scheduler.stop_time == compare_scheduler.stop_time) && (scheduler.interval_time == compare_scheduler.interval_time))
		{
			//Discard scheduler
			ESP_LOGI(TAG, "Discard scheduler Day %d - Start %d - Stop %d - Interval %d - Index %d", day, scheduler.start_time, scheduler.stop_time, scheduler.interval_time, scheduler.index);
			m_scheduler.day[day].scheduler[j].index = scheduler.index;
			return;
		}
		if((compare_scheduler.index == scheduler.index))
		{
			m_scheduler.day[day].scheduler[j] = scheduler;
			return;
		}
	}

	if (index_scheduler < APP_SUPPORT_SIZE)
	{
		m_scheduler.day[day].scheduler[index_scheduler] = scheduler;
		m_scheduler.day[day].num += 1;
	}
	else
	{
		for (int i = 1; i < APP_SUPPORT_SIZE; i++)
		{
			m_scheduler.day[day].scheduler[i - 1] = m_scheduler.day[day].scheduler[i];
		}
		m_scheduler.day[day].scheduler[APP_SUPPORT_SIZE - 1] = scheduler;
	}	
	//
	//  config_set_u32(app_get_tag(APP_DAY_NUMBER_PREFIX, day, 0), m_scheduler.day[day].num);
	//  for (int j = 0; j < m_scheduler.day[day].num; j++)
	//  {
	//    config_set_u32(app_get_tag(APP_SCHEDUER_START_PREFIX, day, j), m_scheduler.day[day].scheduler[j].start_time);
	//    config_set_u32(app_get_tag(APP_SCHEDUER_STOP_PREFIX, day, j), m_scheduler.day[day].scheduler[j].stop_time);
	//    config_set_u32(app_get_tag(APP_SCHEDUER_INTERVAL_PREFIX, day, j), m_scheduler.day[day].scheduler[j].interval_time);
	//  }
}

void app_get_scheduler(void)
{
	for (int i = 0; i < APP_NUMBER_OF_DAY; i++)
	{
		uint8_t active = m_scheduler.day[i].num;
		for (int j = 0; j < active; j++)
		{
			ESP_LOGI(TAG, "Day %d - Start %d - Stop %d - Interval %d", i, m_scheduler.day[i].scheduler[j].start_time, m_scheduler.day[i].scheduler[j].stop_time, m_scheduler.day[i].scheduler[j].interval_time);
		}
	}
}



scheduler_t app_get_scheduler_base_on_time(void)
{


	ESP_LOGI(TAG, "APP_GET_SCHEDULER_BASE_ON_TIME>>>>>>>>>");
	uint32_t t0 = app_get_current_time();  // 456
	uint8_t active = m_scheduler.day[app_today].num; //total numbers for today
	scheduler_t compare_scheduler;

	ESP_LOGI(TAG, "app_get_scheduler_base_on_time() :: %d and today is :: %d\n ", active, app_today);
	// copy all the schedulers of current day in app_today_schedules
	
	// if(woke_up_first_time)
	// {
	// 	 app_today_active=active;
	// 	scheduler_t copy_scheduler;

	// 	for(int i=0;i<active;i++)
	// 	{
	// 		copy_scheduler = m_scheduler.day[app_today].scheduler[i];

	// 		if(copy_scheduler.active==true)
	// 		{
	// 			app_today_schedules[i]=copy_scheduler;
	// 		}
	// 		else
	// 		{
	// 			app_today_active--;
	// 		}
	// }

	// 		// sorting all the schedules of app_today in ascending order according of the start times
	// 		sort_schedule(app_today_schedules,app_today_active);

	// 	woke_up_first_time = false;


	// }



	sort_schedule(m_scheduler.day[app_today].scheduler,active);

   	compare_scheduler.start_time = 0; compare_scheduler.stop_time = 0;
	


	ESP_LOGI(TAG, "T0 %d", t0);
	int counter = 0;

	// ESP_LOGI(TAG,"Overlapping Starttime is %d and Overlapping stoptime is %d",overlapping_starttime,overlapping_stoptime);	

	// if(overlapping_starttime>0 && t0>=overlapping_starttime)
	// {

	// 	app_today_schedules[index_of_current_schedule].active = false;
	// 	index_of_overlapped_schedule[count_of_overlapped_schedules++] = index_of_current_schedule;
	// 	overlapping_starttime=0;

	// }

	// if(overlapping_stoptime>0 && t0>=overlapping_stoptime)
	// {
	// 	app_today_schedules[index_of_current_schedule].active = false;


	// 	// if(app_today_schedules[index_of_overlapped_schedule[count_of_overlapped_schedules-1]].stop_time>t0)
	// 	// {
	// 	//    app_today_schedules[index_of_overlapped_schedule[count_of_overlapped_schedules--]].active=true;
				
	// 	// }

	// 	for(uint8_t i = 0;i< count_of_overlapped_schedules;i++)
	// 	{
	// 		printf("index is: %d\n",index_of_overlapped_schedule[i]);
	// 		ESP_LOGI(TAG, "At above index schedule is  %d - Start %d - Stop %d - Interval %d", app_today, app_today_schedules[index_of_overlapped_schedule[i]].start_time, app_today_schedules[index_of_overlapped_schedule[i]].stop_time, app_today_schedules[index_of_overlapped_schedule[i]].interval_time);
	// 	}


	// 	ESP_LOGI(TAG, "%d \n",count_of_overlapped_schedules);
	// 	for(uint16_t i=count_of_overlapped_schedules-1;i>=0;i--)
	// 	{

	// 		ESP_LOGI(TAG,"%d %d",i,index_of_overlapped_schedule[i]);

	// 		if(app_today_schedules[index_of_overlapped_schedule[i]].stop_time>t0)
	// 		{
	// 			app_today_schedules[index_of_overlapped_schedule[i]].active = true;
	// 			ESP_LOGI(TAG, "Activated scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, app_today_schedules[index_of_overlapped_schedule[i]].start_time, app_today_schedules[index_of_overlapped_schedule[i]].stop_time, app_today_schedules[index_of_overlapped_schedule[i]].interval_time);
	// 			count_of_overlapped_schedules--;
	// 			if(i!=0)
	// 			{
	// 				overlapping_stoptime=app_today_schedules[index_of_overlapped_schedule[i]].stop_time;
	// 				ESP_LOGI(TAG,"Overlapping stop time is set to %d",overlapping_stoptime);
	// 			}
	// 			else
	// 			{
	// 				overlapping_stoptime=0;
	// 			}
	// 			break;
	// 		}
	// 		//ESP_LOGI(TAG, "Present scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, app_today_schedules[index_of_overlapped_schedule[i]].start_time, app_today_schedules[index_of_overlapped_schedule[i]].stop_time, app_today_schedules[index_of_overlapped_schedule[i]].interval_time);
	// 	}	
	// 	//overlapping_stoptime=0;
	// }	

	uint8_t count_of_best_fit_schedules=0;

	for (counter = 0; counter<active; counter++)
	{
		//compare_scheduler = app_today_schedules[counter];
		compare_scheduler = m_scheduler.day[app_today].scheduler[counter];
		// go with the next scheduler if the current scheduler is not active
		if(compare_scheduler.active==false)
		{
			continue;
		}            
		if(compare_scheduler.active==true && compare_scheduler.start_time<=t0 && compare_scheduler.stop_time>t0)
		{
			best_fit_scheduler_index[count_of_best_fit_schedules++]=counter;
		}
	}

	if(count_of_best_fit_schedules>0)
	{
		ESP_LOGI(TAG,"Count of best fit : %d",count_of_best_fit_schedules);
		scheduler_t best_fit_schedule = m_scheduler.day[app_today].scheduler[best_fit_scheduler_index[count_of_best_fit_schedules-1]];
		scheduler_t same_check_schedule;
		same_check_schedule.start_time=same_check_schedule.stop_time=0;

		for(uint8_t i = count_of_best_fit_schedules-1;i>0;i++)
		{

			same_check_schedule=m_scheduler.day[app_today].scheduler[best_fit_scheduler_index[i-1]];;
			if(best_fit_schedule.start_time==same_check_schedule.start_time)
			{
				best_fit_schedule=same_check_schedule;
			}
			else
			{
				break;
			}

		}

		ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", schedule_day, best_fit_schedule.start_time, best_fit_schedule.stop_time, best_fit_schedule.interval_time);

		m_sleep_schedule.isready = true;
		schedule_day = app_today;
		return best_fit_schedule;

	}
	else if(count_of_best_fit_schedules==0)
	{
		ESP_LOGI(TAG,"Count of best fit ::: %d",count_of_best_fit_schedules);

		uint8_t i=0;
		for(i=0; i<active ; i++)
		{
			compare_scheduler = m_scheduler.day[app_today].scheduler[i];
			if(!compare_scheduler.active)
			{
				continue;
			}

			if(compare_scheduler.start_time>t0)
			{	
				ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", schedule_day, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
				m_sleep_schedule.isready = true;
				schedule_day = app_today;
				return compare_scheduler;
			}
		}

		ESP_LOGI(TAG, "Ahiya thi crash thai jay che! \n");

	    // if((i >= active) && (i != 0)) {
		// schedule_day = (app_today+1) % 7;
		// while(m_scheduler.day[schedule_day].num == 0)
		// 	schedule_day = (schedule_day + 1) % 7;
		// if(m_scheduler.day[schedule_day].num > 0){
		// 	compare_scheduler = m_scheduler.day[schedule_day].scheduler[0];
		// 	ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", schedule_day, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
		// 	m_sleep_schedule.isready = false;
		// }
		// }
		// if((i >= active) && (i != 0)){
			schedule_day = (app_today+1) % 7;
			bool do_next_run = true;
			int loop_counter = 0;
			while(do_next_run && loop_counter <= 7){
				loop_counter++;
				if(m_scheduler.day[schedule_day].num == 0 )
				{
					schedule_day = (schedule_day + 1) % 7;
					do_next_run = true;
					continue;
				}
				for(int l = 0; l < m_scheduler.day[schedule_day].num; l++){
					if(m_scheduler.day[schedule_day].scheduler[l].active){
						ESP_LOGI(TAG, "Found the next schedule day %d \n", schedule_day);
						do_next_run = false;
						break;
					}
				}
			}
			
			if(m_scheduler.day[schedule_day].num > 0){
				compare_scheduler = m_scheduler.day[schedule_day].scheduler[0];
				ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", schedule_day, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
				m_sleep_schedule.isready = false;
			}
			ESP_LOGI(TAG, "EXITING \n");

		// }
		return compare_scheduler;
	}
		// if((compare_scheduler.stop_time < t0))	
		// {
		// 	//Discard this scheduler
		// 	ESP_LOGI(TAG, "Discard scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
		// }

		// else
		// {
		// 	//ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
			
		// 	index_of_current_schedule = counter;

		// 	int i=(counter+1)%app_today_active;

		// 	scheduler_t overlapping_schedule;

		// 	do{

		// 		overlapping_schedule = app_today_schedules[i];

		// 		if(compare_scheduler.start_time<=overlapping_schedule.start_time && overlapping_schedule.active==true)
		// 		{
		// 			if(compare_scheduler.start_time==overlapping_schedule.start_time)
		// 			{
		// 				// ESP_LOGI(TAG, "Overlapping Scheduler Day Start %d - Stop %d - Interval %d",overlapping_schedule.start_time, overlapping_schedule.stop_time, overlapping_schedule.interval_time);

		// 				if(compare_scheduler.stop_time<overlapping_schedule.stop_time)
		// 				{
		// 					overlapping_starttime = compare_scheduler.stop_time;
		// 					overlapping_stoptime = overlapping_schedule.stop_time;
		// 					app_today_schedules[i].start_time=overlapping_starttime;

		// 					ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
		// 					ESP_LOGI(TAG, "Overlapping Scheduler Day Start %d - Stop %d - Interval %d",overlapping_schedule.start_time, overlapping_schedule.stop_time, overlapping_schedule.interval_time);


		// 				}
		// 				else
		// 				{
		// 					overlapping_starttime = overlapping_schedule.stop_time;
		// 					app_today_schedules[index_of_current_schedule].start_time=overlapping_starttime;
		// 					overlapping_stoptime = compare_scheduler.stop_time;
		// 					index_of_current_schedule = i;
		// 					compare_scheduler = overlapping_schedule;
		// 					ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
		// 					ESP_LOGI(TAG, "Overlapping Scheduler Day Start %d - Stop %d - Interval %d",overlapping_schedule.start_time, overlapping_schedule.stop_time, overlapping_schedule.interval_time);



		// 				}
		// 			}

		// 			else
		// 			{
		// 			ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
		// 			ESP_LOGI(TAG, "Overlapping Scheduler Day Start %d - Stop %d - Interval %d",overlapping_schedule.start_time, overlapping_schedule.stop_time, overlapping_schedule.interval_time);
		// 			overlapping_starttime = overlapping_schedule.start_time;
		// 			overlapping_stoptime = overlapping_schedule.stop_time;

		// 			// if overlapping scheduler suits according to the current time then choose that scheduler
		// 			// if(t0>=overlapping_starttime && overlapping_starttime>0)
		// 			// {

		// 			// 	app_today_schedules[index_of_current_schedule].active = false;
		// 			// 	index_of_overlapped_schedule[count_of_overlapped_schedules++] = index_of_current_schedule;
		// 			// 	overlapping_starttime=0;
		// 			// 	compare_scheduler = overlapping_schedule;
		// 			// }


		// 			}

		// 			ESP_LOGI(TAG,"Overlapping Starttime is %d and Overlapping stoptime is %d",overlapping_starttime,overlapping_stoptime);	

		// 			break;
		// 		}

		// 		i++;

		// 	}while(i<app_today_active);

		// 	// array index out of bound problem

		// 	m_sleep_schedule.isready = true;
		// 	schedule_day = app_today;
		// 	break;
		// }
	//} // end of the for loop

	//app_today_active = active;
	// if((counter >= active) && (active != 0)){
	// 	schedule_day = (app_today+1) % 7;
	// 	while(m_scheduler.day[schedule_day].num == 0)
	// 		schedule_day = (schedule_day + 1) % 7;
	// 	if(m_scheduler.day[schedule_day].num > 0){
	// 		compare_scheduler = m_scheduler.day[schedule_day].scheduler[0];
	// 		ESP_LOGI(TAG, "Best fit scheduler Day %d - Start %d - Stop %d - Interval %d", schedule_day, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
	// 		m_sleep_schedule.isready = false;
	// 	}
	// }

	// return compare_scheduler;
}	


void app_check_input_scheduler(scheduler_t scheduler)
{
	//Check if input scheduler fit our current schedulers
	uint8_t active = m_scheduler.day[app_today].num;
	scheduler_t compare_scheduler;
	for (int j = 0; j < active; j++)
	{
		compare_scheduler = m_scheduler.day[app_today].scheduler[j];
		if((scheduler.start_time > compare_scheduler.start_time) && (scheduler.start_time < compare_scheduler.stop_time))
		{
			//Discard scheduler
			ESP_LOGI(TAG, "Discard scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, scheduler.start_time, scheduler.stop_time, scheduler.interval_time);
			return;
		}
		else if((scheduler.start_time < compare_scheduler.start_time) && (scheduler.stop_time > compare_scheduler.start_time))
		{
			//Discard scheduler
			ESP_LOGI(TAG, "Discard scheduler Day %d - Start %d - Stop %d - Interval %d", app_today, scheduler.start_time, scheduler.stop_time, scheduler.interval_time);
			return;
		}
		else
		{
			//Scheduler pass
		}
	}

	//Add input scheduler to scheduler list
	app_set_scheduler(app_today, scheduler);
	app_arragne_scheduler_list();
	return;
}

void app_remove_scheduler_pass(uint8_t index_del)
{

	ESP_LOGI(TAG, "removing schedule");

	for(int day_r=0; day_r < APP_NUMBER_OF_DAY; day_r++)
	{	
		uint8_t active = m_scheduler.day[day_r].num;	

		ESP_LOGI(TAG,"Active schedules for day %d are %d",day_r,active);	
		scheduler_t compare_scheduler;

		for (int j = 0; j < active; j++)
		{
			compare_scheduler = m_scheduler.day[day_r].scheduler[j];	


			if(compare_scheduler.index == index_del)
			{	
	
				if(active == 1)
				{
					m_scheduler.day[day_r].num = 0;
					ESP_LOGI(TAG, "zero schedule left at day : %i\r\n", day_r);
					break;
				}
				else
				{
					//Discard scheduler
					ESP_LOGI(TAG, "Removing scheduler Day %d - Start %d - Stop %d - Interval %i", day_r, compare_scheduler.start_time, compare_scheduler.stop_time, compare_scheduler.interval_time);
					app_remove_scheduler(day_r, j);
					break;				
				}
			}
		}
	}
	return;
}


void app_arragne_scheduler_list(void)
{
	uint8_t active = m_scheduler.day[app_today].num - 1;
	uint8_t remain = m_scheduler.day[app_today].num;
	day_scheduler_t newscheduler;
	uint8_t currentindex = 0;

	for (int j = 0; j < active; j++)
	{
		currentindex = 0;
		for (int i = 0; i < remain; i++)
		{
			if (m_scheduler.day[app_today].scheduler[currentindex].start_time > m_scheduler.day[app_today].scheduler[i].start_time)
			{
				currentindex = i;
			}
			else
			{
			}
		}
		remain = remain - 1;
		newscheduler.scheduler[j] = m_scheduler.day[app_today].scheduler[currentindex];
		app_remove_scheduler(app_today, currentindex+1);
		//    ESP_LOGI(TAG, "Current Index %d", currentindex);
		//    app_get_scheduler();
	}
	newscheduler.scheduler[active] = m_scheduler.day[app_today].scheduler[0];

	//  ESP_LOGI(TAG, "Done");

	//Update data for scheduler list
	newscheduler.num = active + 1;
	m_scheduler.day[app_today] = newscheduler;
	return;
}

void app_remove_scheduler(uint8_t day_r, uint8_t position)
{
	uint8_t active = m_scheduler.day[day_r].num;
	day_scheduler_t currentscheduler = m_scheduler.day[day_r];
	day_scheduler_t newscheduler;
	uint8_t i = 0;

	//Check input position
	if(position > active)
	{
		ESP_LOGE(TAG, "There aren't position %d!", position);
		return;
	}

	//Remove select position
	for (int j = 0; j < active; j++)
	{
		if (position == j)
		{
			i = 1;
			
			ESP_LOGE(TAG, "position:  %d!", position);
			//Remove current position
		}
		else
		{
		// 	if((i ==1) && (currentscheduler.scheduler[j].index > 0) )
		// 		currentscheduler.scheduler[j].index--;
			newscheduler.scheduler[j-i] = currentscheduler.scheduler[j];
			ESP_LOGE(TAG, "start:  %d!", currentscheduler.scheduler[j].start_time);
			ESP_LOGE(TAG, "stop:  %d!", currentscheduler.scheduler[j].stop_time);
			ESP_LOGE(TAG, "interval:  %d!", currentscheduler.scheduler[j].interval_time);
			ESP_LOGE(TAG, "index:  %d!", currentscheduler.scheduler[j].index);
		}
	}
	//Update scheduler list
	newscheduler.num = active - 1;
	m_scheduler.day[day_r] = newscheduler;
	return;
}

void app_set_all_scheduler(app_scheduler_t app_scheduler)
{
	m_scheduler = app_scheduler;
}

app_scheduler_t app_get_all_scheduler(void)
{
	return m_scheduler;
}

void app_init_deep_sleep(DEEP_SLEEP_CAUSE_E cause, uint32_t sleeptime)
{	
	const char* func_tag = "app_init_deep_sleep";
	//	Because RTC IO module is enabled in external wake up mode,
	//	internal pullup or pulldown resistors can also be used.
	//	They need to be configured by the application using rtc_gpio_pullup_en() - esp32s2 doc
	esp_sleep_enable_ext0_wakeup(BUTTON_WAKEUP_GPIO, 0);
	rtc_gpio_pullup_en(BUTTON_WAKEUP_GPIO);
	switch (cause) {
	case DEEP_SLEEP_BOTH:
		esp_sleep_enable_timer_wakeup(sleeptime * APP_S_TO_MS_FACTOR);
		ESP_LOGI(TAG, "Wake up by button and timer");
		break;
	case DEEP_SLEEP_BUTTON:
		ESP_LOGI(TAG, "Wake up by button");
		break;
	case DEEP_SLEEP_TIMER:
		esp_sleep_enable_timer_wakeup(sleeptime * APP_S_TO_MS_FACTOR);
		ESP_LOGI(TAG, "Wake up by timer");

		ESP_LOGI(TAG, "Wakeup time: %u", sleeptime * APP_S_TO_MS_FACTOR);


		ESP_LOGI(TAG, "Sleep time:-----------> %u", sleeptime);
		ESP_LOGI(TAG, "APP_S_TO_MS_FACTOR:-----------> %u", APP_S_TO_MS_FACTOR);
		break;
	default:
		ESP_LOGI(TAG, "Deep sleep defaul mode!");
		break;
	}
	//backup_scheduler_now();
	esp_wifi_set_mode(WIFI_MODE_NULL);
	ESP_LOGI(func_tag, "Sleep time: %u", sleeptime/1000);
	esp_wifi_stop();	
	esp_deep_sleep_start();
}

void app_job(void)
{
	//Get current time
	uint32_t t0 = app_get_current_time();
	uint32_t current_millis = millis();
	ESP_LOGI(TAG, "************** inside Current time: %d  **************", t0);
	ESP_LOGI(TAG, "************** Current time: %d  **************", current_millis);
	ESP_LOGI(TAG, "****************************m_sleep_schedule*******************: %d", m_sleep_schedule.sleep_scheduler.start_time);
  
  
	if(m_sleep_schedule.sleep_scheduler.start_time <= t0){
		uint32_t remain_time = m_sleep_schedule.sleep_scheduler.stop_time - t0;
		
		if(m_sleep_schedule.sleep_scheduler.interval_time == 0)
		{
			m_sleep_schedule.sleep_time = (m_sleep_schedule.sleep_scheduler.stop_time - m_sleep_schedule.sleep_scheduler.start_time);
		}
		else if (remain_time < (m_sleep_schedule.sleep_scheduler.interval_time*60))
		{
			m_sleep_schedule.isready = false;
			m_sleep_schedule.isrunning = false;
			//Sleep time = remain time - 6;
			//m_sleep_schedule.sleep_time = remain_time - 6;
			m_sleep_schedule.sleep_time = remain_time;
			ESP_LOGI(TAG, "****************************Sleep time*******************: %d", m_sleep_schedule.sleep_time);
			ESP_LOGI(TAG, "******************* Last round *******************");
		}
		else
		{
			if(m_sleep_schedule.isrunning) //Not first time
			{
				//Sleep time = last start time + 2 * interval - 6 - current time
				//m_sleep_schedule.sleep_time = 2 * m_sleep_schedule.sleep_scheduler.interval_time + m_sleep_schedule.last_start_time - 6 - t0;
				m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.interval_time*60;
				ESP_LOGI(TAG, "****************************Sleep time0*******************: %d", m_sleep_schedule.sleep_time);
			}
			else //First time
			{
				//Sleep time = scheduler start time + interval - 6 - current time
				//m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.interval_time + m_sleep_schedule.sleep_scheduler.start_time - 6 - t0;
				m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.interval_time*60;

				m_sleep_schedule.isrunning = true;
				ESP_LOGI(TAG, "****************************Sleep time1*******************: %d", m_sleep_schedule.sleep_time);
			}
		}

		//Take start time
		m_sleep_schedule.last_start_time = app_get_current_time();
		ESP_LOGI(TAG, "Last start time: %i", m_sleep_schedule.last_start_time);
		ESP_LOGI(TAG, "*****Start motor task*****\n");
		for (int count = 0; count < 1; count++) {
			gpio_set_level(MOTOR_OUT, 1);
			ESP_LOGI(TAG, "Motor ON! %d", count);
			vTaskDelay(470/portTICK_RATE_MS);
			gpio_set_level(MOTOR_OUT, 0);
			ESP_LOGI(TAG, "Motor OFF! %d", count);
			vTaskDelay(1530/portTICK_RATE_MS);
		}
		ESP_LOGI(TAG, "*****Stop motor task*****\n");

		 bool status = nvs_read_data("spray_counter");
		 
		 ESP_LOGI(TAG, "*****Status***** %d \n", status);

            if(status == 0){
              remaining_sprays = run_time[0];
            } 
            else{
            	ESP_LOGI(TAG,"Lost remaining spray count from the nvs memory");
            }
		if(remaining_sprays>0)	
			remaining_sprays--;
		nvs_write_value("spray_counter",remaining_sprays, 0);


		ESP_LOGI(TAG,"********Remaining Sprays**********: %d",remaining_sprays);
		// bool decrease_spray_count = nvs_read_data("Spray_info");  
		// if(decrease_spray_count)
		// {
		// 	uint32_t current_spray_count = run_time[0];
		// 	current_spray_count--;
		// 	ESP_LOG(TAG,"Current available sprays: %u",current_spray_count);
		// 	nvs_write_value("Spray_info",current_spray_count, 0);			
		// }
		// else
		// {
		// 	ESP_LOGI(TAG,"Failed to retrieve spray count from the nvs memory");
		// }


	  
	}
	else {
		m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.start_time - t0;
		ESP_LOGI(TAG, "****************************Sleep time2*******************: %d", m_sleep_schedule.sleep_time);
	}

	
	//nvs_write_value("spray_counter",m_sleep_schedule.sleep_time, 0);
	t0 = millis();
	uint32_t t0_diff = t0 - old_millis;
	ESP_LOGI(TAG, "#####T0#####: %d", t0);
	ESP_LOGI(TAG, "#####Old_millis#####: %d", old_millis);

	// m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.start_time - t0;

	// ESP_LOGI(TAG, "**************  Current time: %d  **************", t0);
	// ESP_LOGI(TAG, "**************  diff between old and current time: %d  **************", t0_diff);

	//current_millis = millis() - current_millis;
	ESP_LOGI(TAG, "Absolute Sleep time: %d", m_sleep_schedule.sleep_time);
	if(m_sleep_schedule.sleep_time  < 4)
		m_sleep_schedule.sleep_time = 4;
	uint32_t temp_sleep_time = m_sleep_schedule.sleep_time;
	m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_time * 1000;
	// m_sleep_schedule.sleep_time  = m_sleep_schedule.sleep_time - ( temp_sleep_time * 180 );
	// app_init_deep_sleep(DEEP_SLEEP_TIMER, m_sleep_schedule.sleep_time - t0_diff);
     
     int DIFF_TIME ;
     if(m_sleep_schedule.sleep_time < t0){

           DIFF_TIME = t0_diff - m_sleep_schedule.sleep_time ;

          ESP_LOGI(TAG, ":::::::::::DIFF_TIME:::::::::: %d", DIFF_TIME);

     }else{
     
          DIFF_TIME =  m_sleep_schedule.sleep_time - t0_diff;
 }
     
     ESP_LOGI(TAG, "#####sleep time#####: %d", m_sleep_schedule.sleep_time - t0_diff);
     ESP_LOGI(TAG, "#####m_sleep_schedule.sleep_time#####: %d", m_sleep_schedule.sleep_time);

     // ESP_LOGI(TAG, "#####m_sleep_schedule.sleep_time - t0_diff#####: %d", m_sleep_schedule.sleep_time - t0_diff);
     ESP_LOGI(TAG, "#####DIFF_TIME#####: %d", DIFF_TIME);

     if(DIFF_TIME < ZERO){

        DIFF_TIME = 13*60*1000;
        ESP_LOGI(TAG, "********#####DIFF_TIME#####*******: %d", DIFF_TIME);


     }
     // ESP_LOGI(TAG, "#####*************DIFF_TIME*************#####: %d", DIFF_TIME);

 if(DIFF_TIME>50000){
         

         ESP_LOGI(TAG, "********#####DIFF_TIME > 50000#####*******");

         app_init_deep_sleep(DEEP_SLEEP_TIMER, m_sleep_schedule.sleep_time);



 }else{

     ESP_LOGI(TAG, "********#####DIFF_TIME < 50000#####*******");
     app_init_deep_sleep(DEEP_SLEEP_TIMER, DIFF_TIME);
     
 	}

 	// app_init_deep_sleep(DEEP_SLEEP_TIMER, DIFF_TIME);

}

void app_init_scheduler(void)
{
	ESP_LOGI(TAG, "*****Wake up*****");
	uint32_t t0,current_millis;
	m_sleep_schedule.sleep_scheduler = app_get_scheduler_base_on_time();

	//m_sleep_schedule.isready = true; 

	if(m_sleep_schedule.isready) //Already init scheduler
	{
		printf("Already init scheduler!\n");
		app_job();
	}
	else //Didn't init scheduler
	{
		if((m_sleep_schedule.sleep_scheduler.start_time != 0) || (m_sleep_schedule.sleep_scheduler.stop_time != 0))
		{
			//Sleep time = scheduler start time - current time
			int days_ahead = 0;
			if(schedule_day > app_today)
				days_ahead = schedule_day - app_today - 1;
			else if(schedule_day != 0)
				days_ahead = (schedule_day-1) + (6-app_today);
			else
				days_ahead = (6 - app_today);

			ESP_LOGI(TAG,"days_ahead: %i \n", days_ahead);
			m_sleep_schedule.sleep_time = (86399 - app_get_current_time()) + m_sleep_schedule.sleep_scheduler.start_time + 86399 * days_ahead;
			//m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.sleep_time;
			//ESP_LOGI(TAG, "Sleep time till start: %d", m_sleep_schedule.sleep_time);
			//m_sleep_schedule.remain_time =  m_sleep_schedule.sleep_scheduler.stop_time - m_sleep_schedule.sleep_scheduler.start_time;
			// ESP_LOGI(TAG, "Sleep time: %d", m_sleep_schedule.sleep_time);
			uint32_t new_time = app_get_current_time();
			t0 = millis();
			t0 = t0 - old_millis;

			int time_diff;

			// if(m_sleep_schedule.sleep_time>t0){

                 time_diff = m_sleep_schedule.sleep_time - t0;
                 // ESP_LOGI(TAG, "time_diff::::: %d", time_diff);

			// } else if(t0> m_sleep_schedule.sleep_time){

   //               time_diff = t0 - m_sleep_schedule.sleep_time ;
   //               ESP_LOGI(TAG, ":::::::::::time_diff::::::::: %d", time_diff);

			// }

				if(time_diff<0){

	                  m_sleep_schedule.sleep_time = 13 * 60;
				     
				            }
      
			// m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_scheduler.start_time - t0;
            ESP_LOGI(TAG, "##############  t0: %d  ##############", t0);
            ESP_LOGI(TAG, "##############  m_sleep_schedule.sleep_time: %d  ##############", m_sleep_schedule.sleep_time);
            ESP_LOGI(TAG, "##############  old_millis: %d  ##############", old_millis);
			ESP_LOGI(TAG, "**************  Current time: %d  **************", new_time);
			//current_millis = millis() - old_millis;
			ESP_LOGI(TAG, "Absolute Sleep time: %d", m_sleep_schedule.sleep_time);
			// ESP_LOGI(TAG, " Sleep time: %d", time_diff);
			uint32_t temp_sleep_time = m_sleep_schedule.sleep_time;
			m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_time * 1000;
			ESP_LOGI(TAG, " Sleep time: %d", m_sleep_schedule.sleep_time-t0);
			// m_sleep_schedule.sleep_time  = m_sleep_schedule.sleep_time - ( temp_sleep_time * 180 );

		 //    ESP_LOGI(TAG, "#####Sleep time1#####: %d", time_diff);

			// app_init_deep_sleep(DEEP_SLEEP_BOTH, time_diff);

           

           ESP_LOGI(TAG, "#####Sleep time1#####: %d", m_sleep_schedule.sleep_time-t0);

			app_init_deep_sleep(DEEP_SLEEP_BOTH, m_sleep_schedule.sleep_time-t0);
			

		}
		else
		{	
			m_sleep_schedule.sleep_time = -1;
			t0 = millis();
			t0 = t0 - old_millis;
			m_sleep_schedule.sleep_time = 13 * 60;
	
			ESP_LOGI(TAG, "Wake up by Button: %d", m_sleep_schedule.sleep_time);
			ESP_LOGI(TAG, "Absolute Sleep time: %d", m_sleep_schedule.sleep_time);
			
			uint32_t temp_sleep_time = m_sleep_schedule.sleep_time;
			m_sleep_schedule.sleep_time = m_sleep_schedule.sleep_time * 1000;
			// m_sleep_schedule.sleep_time  = m_sleep_schedule.sleep_time - ( temp_sleep_time * 180 );
            ESP_LOGI(TAG, " Sleep time: %d", m_sleep_schedule.sleep_time - t0);
			ESP_LOGI(TAG, "#####sleep time2#####: %d", m_sleep_schedule.sleep_time - t0);

			app_init_deep_sleep(DEEP_SLEEP_BOTH, m_sleep_schedule.sleep_time - t0);
			// app_init_deep_sleep(DEEP_SLEEP_BOTH, m_sleep_schedule.sleep_time);
		}	
	}
}

uint32_t app_get_current_time( void )
{
	time_t now;
	char strftime_buf[64];
	struct tm timeinfo;
    retrive_gmt();
    ESP_LOGI(TAG, "**********<<<<<<<<<<<READ RTC DATA>>>>>>>>>>>>>>*********");

    
	//time(&now);

	now  = time(NULL);

	ESP_LOGI(TAG, "Epoch is %u", now);
	//ESP_LOGI(TAG, "The current time zone epoch is: %u", now);

	if(print_time_zone[0]!=0)
		ESP_LOGI(TAG, "The current time zone is: %s", print_time_zone);
	
	// Set timezone to China Standard Time
	setenv("TZ", time_zone, 1);		
	tzset();
	
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

	if(print_time_zone[0]==0)
		ESP_LOGI(TAG, "The current date/time in %s is: %s", time_zone, strftime_buf);
	else
		ESP_LOGI(TAG, "The current date//time in %s is: %s", print_time_zone, strftime_buf);
	ESP_LOGW(TAG, "The current date///time is: %s", strftime_buf);
	
	//if(strstr(strftime_buf, ""))
	//struct timeval tv_now;
	//gettimeofday(&tv_now, NULL);
	//uint32_t t0 = ((uint32_t)tv_now.tv_sec)%(24*60*60);
	//return t0;
	if(strstr(strftime_buf, "Mon"))
		app_today = 0;
	else if(strstr(strftime_buf, "Tue"))
		app_today = 1;
	else if(strstr(strftime_buf, "Wed"))
		app_today = 2;
	else if(strstr(strftime_buf, "Thu"))
		app_today = 3;
	else if(strstr(strftime_buf, "Fri"))
		app_today = 4;
	else if(strstr(strftime_buf, "Sat"))
		app_today = 5;
	else if(strstr(strftime_buf, "Sun"))
		app_today = 6;
	
	ESP_LOGI(TAG, "Today is : %i \r\n", app_today);
	printf("strftime_buf: %s \n",strftime_buf);

	return tokenize_string(strftime_buf);

}

static uint32_t tokenize_string(char *str)
{
    int i = 0;
	char *ptr[8];
	char *date[8];
	char delim = ' ';
	char delim1 = ':';
    ptr[i] = strtok(str,&delim);

    while ((ptr[i] != NULL))
    {
#ifdef DEBUG_MORE
        printf("'%s'\n", ptr[i]);
#endif
        i++;
        if (i < 8)
            ptr[i] = strtok(NULL, &delim);
        else
            break;
    }

	i = 0;
	date[i] = strtok(ptr[3] , &delim1);

	while ((date[i] != NULL))
    {
#ifdef DEBUG_MODE
        printf("'%s'\n", date[i]);
#endif
        i++;
        if (i < 3)
            date[i] = strtok(NULL, &delim1);
        else
            break;
    }

	uint32_t hours = strtol(ptr[3], &ptr[3], 10);
	uint32_t minutes = strtol(ptr[4], &ptr[4], 10);
	uint32_t seconds = strtol(ptr[5], &ptr[5], 10);

	uint32_t net_secs = hours*3600+ minutes*60 + seconds;
#ifdef DEBUG_MODE
	printf("Hours: %d \n",hours);
	printf("Minutes: %d \n",minutes);
	printf("Seconds: %d \n",seconds);
	printf("Net seconds since midnight: %d \n",net_secs);
#endif
	return net_secs;
}
