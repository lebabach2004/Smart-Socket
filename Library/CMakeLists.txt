set(app_src app_config.c app_mqtt.c http_server_app.c app_uart.c app_time.c)
set(pri_req esp_wifi nvs_flash mqtt esp_http_server esp_driver_gpio json_generator json_parser esp_driver_gpio esp_driver_uart protocol_examples_common)
idf_component_register(SRCS ${app_src} 
                       INCLUDE_DIRS "."
                       REQUIRES ${pri_req})