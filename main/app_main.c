/* MQTT Mutual Authentication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "app_config.h"
#include "app_mqtt.h"
#include "driver/gpio.h"
#include "app_time.h"
#include "time.h"

#include "driver/uart.h"
#include "app_uart.h"
static const char *TAG = "Sonoff";
#define KEY "restart_cnt"
#define KEY1 "string"
#define UART_BUF_SIZE (1024)
typedef struct{
    int hour;
    int min;
    int sec;
}time_setting_t;
static time_setting_t time_start;
static time_setting_t time_end;
static int timer_setting_flag=0;
static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if(event_base==MQTT_DEV_EVENT){
        switch (event_id)
        {
        case MQTT_DEV_EVENT_CONNECTED:
            ESP_LOGW(TAG, "MQTT_DEV_EVENT_CONNECTED");
            break;
        case MQTT_DEV_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_DEV_EVENT_DISCONNECTED");
            break;
        case MQTT_DEV_EVENT_SUBSCRIBED:
            ESP_LOGW(TAG, "MQTT_DEV_EVENT_SUBSCRIBED");
            break;
        case MQTT_DEV_EVENT_PUBLISHED:
            ESP_LOGW(TAG, "MQTT_DEV_EVENT_PUBLISHED");
            break;
        case MQTT_DEV_EVENT_DATA:
            ESP_LOGW(TAG, "MQTT_DEV_EVENT_DATA");
            break;
        default:
            break;
        }
    }
}
void mqtt_data_callback(char *buf,int len){
    if(strstr(buf,"ON")){
        gpio_set_level(GPIO_NUM_2,1);
        gpio_set_level(GPIO_NUM_23,1);
    }
    else if(strstr(buf,"OFF")){
        gpio_set_level(GPIO_NUM_2,0);
        gpio_set_level(GPIO_NUM_23,0);
    }
}
void mqtt_time_start_callback(char *buf,int len){
    buf[len]='\0';
    char *p;
    p=strtok(buf,":");
    time_start.hour=atoi(p);
    p=strtok(NULL,":");
    time_start.min=atoi(p);
    p=strtok(NULL,":");
    time_start.sec=atoi(p);
    printf("Start time: %d %d %d\n",time_start.hour,time_start.min,time_start.sec);
}
void mqtt_time_end_callback(char *buf,int len){
    buf[len]='\0';
    char *p;
    p=strtok(buf,":");
    time_end.hour=atoi(p);
    p=strtok(NULL,":");
    time_end.min=atoi(p);
    p=strtok(NULL,":");
    time_end.sec=atoi(p);
    printf("End time: %d %d %d\n",time_end.hour,time_end.min,time_end.sec);
}
void mqtt_timer_callback(char *buf,int len){
    if(strstr(buf,"ON")){
        timer_setting_flag=1;
    }
    else if(strstr(buf,"OFF")){
        timer_setting_flag=0;
    }
}
bool check_range_time(void){
    int now_sec=timeinfo.tm_hour*3600+timeinfo.tm_min*60+timeinfo.tm_sec;
    int start_sec=time_start.hour*3600+time_start.min*60+time_start.sec;
    int end_sec=time_end.hour*3600+time_end.min*60+time_end.sec;
    printf("Start time: %d End time:%d Current time:%d\n",start_sec,end_sec,now_sec);
    if(start_sec<end_sec){
        if(now_sec>=start_sec && now_sec<=end_sec){
            return true;
        }
    }
    else{
        if(now_sec>=start_sec || now_sec<=end_sec){
            return true;
        }
    }
    return false;
}
void time_current(void *pvParameter){
    for(;;){
        display_time();
        printf("Timer setting flag: %d\n",timer_setting_flag);
        if(timer_setting_flag){
            if(check_range_time() && gpio_get_level(GPIO_NUM_23)==1){
                set_low_public_relay(GPIO_NUM_23);
            }
            else if(!check_range_time() && gpio_get_level(GPIO_NUM_23)==0){
                set_high_public_relay(GPIO_NUM_23);
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void uart_echo_task(void *arg) {
    int uart_num = UART_NUM_2;
    uint8_t *data = (uint8_t *) malloc(UART_BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(uart_num, data, (UART_BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        uart_write_bytes(uart_num, (const char *) data, len);
        if (len) {
            data[len] = '\0';
            printf("UART: %s\n", data);
            float power_value = atof((char *) data)*220;
            char power_str[20];
            sprintf(power_str, "%.2f", power_value); 
            set_public_current_value((char *) data);
            set_public_power_value((char *) power_str);
        }
    }
}
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());


    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK( esp_event_handler_register(MQTT_DEV_EVENT,ESP_EVENT_ANY_ID, &event_handler, NULL) );
    gpio_set_direction(GPIO_NUM_2,GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(GPIO_NUM_23,GPIO_MODE_INPUT_OUTPUT);
    app_mqtt_set_data_callback(mqtt_data_callback);
    app_mqtt_time_start_callback(mqtt_time_start_callback);
    app_mqtt_time_end_callback(mqtt_time_end_callback);
    app_mqtt_timer_callback(mqtt_timer_callback);
    uart_init(UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16);
    app_config();
    init_app_time();
    mqtt_app_start();
    xTaskCreate(time_current,"time_current",4096,NULL,5,NULL);
    xTaskCreate(uart_echo_task, "uart_echo_task", 2048, NULL, 8, NULL);
}