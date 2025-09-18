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
#define JSON_BUFFER_SIZE 128
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
        msg_id = esp_mqtt_client_subscribe(client, "/smart/relay", 0);
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
        msg_id = esp_mqtt_client_publish(client, "/smart/relay", "data", 0, 0, 0);
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
        char data_recv[256];
        snprintf(data_recv, sizeof(data_recv), "%.*s", event->data_len, event->data);
        data_recv[event->data_len]='\0';
        esp_event_post(MQTT_DEV_EVENT, MQTT_DEV_EVENT_DATA, NULL, 0 , portMAX_DELAY);

        jparse_ctx_t jctx;
        if (json_parse_start(&jctx, data_recv, strlen(data_recv)) == OS_SUCCESS){
            if (strcmp(topic_recv, "/bachdz/relay") == 0) {
                char relay_val[10];
                if (json_obj_get_string(&jctx, "relay", relay_val, sizeof(relay_val)) == OS_SUCCESS) {
                    mqtt_data_handle(relay_val,event->data_len);
                }
            }
            if(strcmp(topic_recv,"/start/time")==0){
                char start[16];
                if (json_obj_get_string(&jctx, "start", start, sizeof(start)) == OS_SUCCESS) {
                    mqtt_data_time_start(start,event->data_len);
                }
            }
            if(strcmp(topic_recv,"/end/time")==0){
                char end[16];
                if (json_obj_get_string(&jctx, "end", end, sizeof(end)) == OS_SUCCESS) {
                    mqtt_data_time_end(end,event->data_len);
                }
            }
            if(strstr(topic_recv,"/timer/")){
                char timer_val[20];
                if (json_obj_get_string(&jctx, "time", timer_val, sizeof(timer_val)) == OS_SUCCESS) {
                    mqtt_data_timer(timer_val,event->data_len);
                }
            }
            json_parse_end(&jctx);
        }
        else {
            ESP_LOGE(TAG, "JSON Parse Error");
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
    char buf[JSON_BUFFER_SIZE];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "relay", "ON");
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    esp_mqtt_client_publish(client, "/smart/turn", buf, 0, 1, 0);
}
void set_low_public_relay(int gpio_num){
    gpio_set_level(gpio_num,0);
    char buf[JSON_BUFFER_SIZE];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "relay", "OFF");
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    esp_mqtt_client_publish(client, "/smart/turn", buf, 0, 1, 0);
}
void set_public_power_value(char *data){
    char buf[JSON_BUFFER_SIZE];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "power", data);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    esp_mqtt_client_publish(client, "/device/power", buf, 0, 1, 0);
}
void set_public_current_value(char *data){
    char buf[JSON_BUFFER_SIZE];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, sizeof(buf), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "current", data);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    esp_mqtt_client_publish(client, "/device/current", buf, 0, 1, 0);
}