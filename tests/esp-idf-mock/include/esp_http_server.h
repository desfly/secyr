#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
using ssize_t = std::ptrdiff_t;
using httpd_handle_t = void*;
struct httpd_req_t { void* user_ctx; };
using httpd_uri_func_t = esp_err_t(*)(httpd_req_t*);
enum { HTTP_GET=0, HTTPD_500_INTERNAL_SERVER_ERROR=500 };
struct httpd_uri_t { const char* uri; int method; httpd_uri_func_t handler; void* user_ctx; };
struct httpd_config_t { int max_uri_handlers; int stack_size; bool lru_purge_enable; };
inline httpd_config_t HTTPD_DEFAULT_CONFIG(){ return {8,4096,false}; }
inline esp_err_t httpd_start(httpd_handle_t* h,const httpd_config_t*){*h=(void*)1;return ESP_OK;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*){return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t*,const char*,std::ptrdiff_t){return ESP_OK;}
inline esp_err_t httpd_resp_send_err(httpd_req_t*,int,const char*){return ESP_OK;}
