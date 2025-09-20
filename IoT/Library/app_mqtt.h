#ifndef __MQTT_APP__H
#define __MQTT_APP__H
typedef struct{
    char buf[256];
    size_t offset;
} json_gen_test_result_t;
ESP_EVENT_DECLARE_BASE(MQTT_DEV_EVENT);
enum{
    MQTT_DEV_EVENT_CONNECTED,
    MQTT_DEV_EVENT_DISCONNECTED,
    MQTT_DEV_EVENT_SUBSCRIBED,
    MQTT_DEV_EVENT_UNSUBSCRIBED,
    MQTT_DEV_EVENT_PUBLISHED,
    MQTT_DEV_EVENT_DATA,
};
void mqtt_app_start(void);
typedef void (*mqtt_data_handle_t)(char *buf,int len);
void app_mqtt_set_data_callback(void *cb);
void app_mqtt_time_start_callback(void *cb);
void app_mqtt_time_end_callback(void *cb);
void app_mqtt_timer_callback(void *cb);
void json_gen_test(json_gen_test_result_t *result,char *key1,bool value1,char *key2,int value2, char *key3, char *value3);
void json_gen_test_arr(json_gen_test_result_t *result, char *key1,char *value1,char *key2, int *value2,int size);
void set_high_public_relay(int gpio_num);
void set_low_public_relay(int gpio_num);
void set_public_power_value(char *data);
void set_public_current_value(char *data);
#endif