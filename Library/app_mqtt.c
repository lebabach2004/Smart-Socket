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
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "json_generator.h"
#include "json_parser.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "app_mqtt.h"
static const char *TAG = "mqtts_example";
ESP_EVENT_DEFINE_BASE(MQTT_DEV_EVENT);
extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_mosquitto_org_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_mosquitto_org_crt_end");
static mqtt_data_handle_t mqtt_data_handle = NULL; 
static mqtt_data_handle_t mqtt_data_time_start = NULL; 
static mqtt_data_handle_t mqtt_data_time_end = NULL; 
static mqtt_data_handle_t mqtt_data_timer = NULL;
esp_mqtt_client_handle_t client;
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/bachdz/relay", 0);
        msg_id = esp_mqtt_client_subscribe(client, "/start/time", 0);
        msg_id = esp_mqtt_client_subscribe(client, "/end/time", 0);
        msg_id = esp_mqtt_client_subscribe(client, "/timer/", 0);
        msg_id= esp_mqtt_client_publish(client, "/bachdz/123", "helloword" , 0, 1, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        esp_event_post(MQTT_DEV_EVENT, MQTT_DEV_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        esp_event_post(MQTT_DEV_EVENT, MQTT_DEV_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/bachdz/relay", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        esp_event_post(MQTT_DEV_EVENT, MQTT_DEV_EVENT_SUBSCRIBED, NULL, 0, portMAX_DELAY);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        char topic_recv[20];
        strncpy(topic_recv,event->topic,event->topic_len);
        topic_recv[event->topic_len]='\0';
        esp_event_post(MQTT_DEV_EVENT, MQTT_DEV_EVENT_DATA, NULL, 0 , portMAX_DELAY);
        if(strcmp(topic_recv,"/bachdz/relay")==0){
            mqtt_data_handle(event->data,event->data_len);
        }
        if(strcmp(topic_recv,"/start/time")==0){
            mqtt_data_time_start(event->data,event->data_len);
        }
        if(strcmp(topic_recv,"/end/time")==0){
            mqtt_data_time_end(event->data,event->data_len);
        }
        if(strstr(topic_recv,"/timer/")){
            mqtt_data_timer(event->data,event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
        // .broker.verification.certificate = (const char *)server_cert_pem_start,
        // .credentials = {
        // .authentication = {
        //     .certificate = (const char *)client_cert_pem_start,
        //     .key = (const char *)client_key_pem_start,
        // },
        // }
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
static void flush_str(char * buf,void *priv){
    json_gen_test_result_t *result = (json_gen_test_result_t *)priv;
    if(result){
        if(strlen(buf)>sizeof(result->buf)-result->offset){
            printf("Result buffer too small\r\n");
            return;
        }
        memcpy(result->buf+result->offset,buf,strlen(buf));
        result->offset+=strlen(buf);
    }
}
void json_gen_test(json_gen_test_result_t *result,char *key1,bool value1,char *key2,int value2, char *key3, char *value3)
{
    char buf[20];
    memset(result,0,sizeof(json_gen_test_result_t));
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), flush_str, result);
    json_gen_start_object(&jstr);
    json_gen_obj_set_bool(&jstr, key1, value1);
    json_gen_obj_set_int(&jstr, key2, value2);
    json_gen_obj_set_string(&jstr, key3, value3);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    printf("Result: %s\r\n",result->buf);
}
void json_gen_test_arr(json_gen_test_result_t *result, char *key1,char *value1,char *key2, int *value2,int size)
{
    char buf[100];  // Tăng kích thước bộ đệm để chứa JSON lớn hơn
    memset(result, 0, sizeof(json_gen_test_result_t));
    int length=size/sizeof(value2[0]);
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), flush_str, result);
    json_gen_start_object(&jstr); // Bắt đầu JSON object
    json_gen_obj_set_string(&jstr, key1, value1);
    json_gen_obj_start_object(&jstr, key2); 
    json_gen_start_array(&jstr); // Bắt đầu mảng dưới key2
    
    for (int i = 0; i < length; i++)
    {
        json_gen_arr_set_int(&jstr, value2[i]); // Thêm phần tử vào mảng
    }

    json_gen_end_array(&jstr); // Kết thúc mảng
    json_gen_end_object(&jstr); // Kết thúc object

    json_gen_str_end(&jstr);
    printf("Result: %s\r\n", result->buf);
}
void app_mqtt_set_data_callback(void *cb){
    mqtt_data_handle = cb;
}
void app_mqtt_time_start_callback(void *cb){
    mqtt_data_time_start = cb;
}
void app_mqtt_time_end_callback(void *cb){
    mqtt_data_time_end = cb;
}
void app_mqtt_timer_callback(void *cb){
    mqtt_data_timer = cb;
}
void set_high_public_relay(int gpio_num){
    gpio_set_level(gpio_num,1);
    esp_mqtt_client_publish(client, "/bachdz/turn", "ON", 0, 1, 0);
}
void set_low_public_relay(int gpio_num){
    gpio_set_level(gpio_num,0);
    esp_mqtt_client_publish(client, "/bachdz/turn", "OFF", 0, 1, 0);
}
void set_public_power_value(char *data){
    esp_mqtt_client_publish(client, "/device/power",data, 0, 1, 0);
}
void set_public_current_value(char *data){
    esp_mqtt_client_publish(client, "/device/current",data, 0, 1, 0);
}