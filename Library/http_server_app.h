#ifndef HTTP_SERVER_APP_H
#define HTTP_SERVER_APP_H
#include "esp_err.h"
#include <esp_http_server.h>
typedef void (*http_post_handler_func_t) (char *data,int len);
typedef void (*http_get_handler_func_t) (char *url_query,char host);
esp_err_t stop_webserver(void);
void start_webserver(void);
void app_http_server_get_set_callback(void*cb);
void app_http_server_post_set_callback(void*cb);
#endif