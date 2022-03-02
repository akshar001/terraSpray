/* ADC2 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include "battery.h"



const float battery_max = 1830; //maximum voltage of battery
const float battery_min = 1445;  //minimum voltage of battery before shutdown


static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}


static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void init_battery(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel,atten);
    

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

}

int get_battery(void)
{

//     printf("start conversion.\n");
//     uint32_t adc_reading = 0;
//     //Multisampling
//     // int raw=0;
//     // for (int i = 0; i < NO_OF_SAMPLES; i++) {

//     //   adc1_get_raw(channel);
//     //   adc_reading += raw;
//     // }
    
//     // adc_reading /= NO_OF_SAMPLES;
//     adc_reading = adc1_get_raw(channel);
//     //Convert adc_reading to voltage in mV
//     float voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
//     printf("Raw: %d\tVoltage: %0.2fmV\n", adc_reading, voltage);
//     float perVlot =  (2*voltage - 2400)/600;

//     if(perVlot <= 1)
//         perVlot *= 100;
//     else 
//         perVlot = 100;
//     printf("Percentage Voltage read is %f \n", perVlot);
//     printf("Percentage Voltage read is %d \n", (int)perVlot);
//     return perVlot;



    
   printf("start conversion.\n");
   uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        if (unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)channel);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)channel, width, &raw);
            adc_reading += raw;
        }
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    float voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    printf("Raw: %d\tVoltage: %0.2fmV\n", adc_reading, voltage);
    float perVlot;

    //voltage = 1600;

    if(voltage>battery_max)
        perVlot=100;
    else if(voltage<battery_min)
        perVlot=0;
    else
    {
        printf("%f",voltage-battery_min);
        perVlot = ((voltage-battery_min)/(battery_max-battery_min))*100;
    } 



    printf("Percentage Voltage read is %f \n", perVlot);
    printf("Percentage Voltage read is %d \n", (int)perVlot);
    return perVlot;

}
