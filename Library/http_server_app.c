#include "http_server_app.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_tls_crypto.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_check.h"
#include "esp_err.h"
#include "driver/gpio.h"
static const char *TAG = "example";
static httpd_handle_t server = NULL;
// extern const uint8_t index_html_start[] asm("_binary_anh_jpg_start");
// extern const uint8_t index_html_end[] asm("_binary_anh_jpg_end");
extern const uint8_t index_html_start[] asm("_binary_test_html_start");
extern const uint8_t index_html_end[] asm("_binary_test_html_end");
static http_post_handler_func_t http_post_handler_func=NULL;
static http_get_handler_func_t http_get_handler_func=NULL;
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,(const char*)index_html_start,index_html_end-index_html_start);
    return ESP_OK;
}

static const httpd_uri_t http_get = {
    .uri       = "/get",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = "NULL"
};


static esp_err_t http_post_handler(httpd_req_t *req)
{
    char buf[100];
    httpd_req_recv(req, buf,req->content_len);
    http_post_handler_func(buf,req->content_len);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t http_post = {
    .uri       = "/post",
    .method    = HTTP_POST,
    .handler   = http_post_handler,
    .user_ctx  = "NULL"
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/get", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        return ESP_OK;
    } 
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server,&http_get);
        httpd_register_uri_handler(server,&http_post);
        httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,http_404_error_handler);
    }
    else{
        ESP_LOGI(TAG, "Error starting server!");
    }
}

esp_err_t stop_webserver(void)
{
    return httpd_stop(server);
}
void app_http_server_post_set_callback(void*cb){
    http_post_handler_func=cb;
}
void app_http_server_get_set_callback(void *cb){
    http_get_handler_func=cb;
}