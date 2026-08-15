#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
using ssize_t = std::ptrdiff_t;
using httpd_handle_t = void*;
struct httpd_req_t { void* user_ctx{}; std::size_t content_len{}; const char* mock_body{}; int method{}; const char* mock_authorization{}; };
using httpd_uri_func_t = esp_err_t(*)(httpd_req_t*);
using httpd_work_fn_t = void(*)(void*);
enum { HTTP_GET=0, HTTP_POST=1, HTTPD_500_INTERNAL_SERVER_ERROR=500, HTTPD_WS_TYPE_TEXT=1, HTTPD_WS_TYPE_CLOSE=2, HTTPD_WS_TYPE_PING=3, HTTPD_WS_TYPE_PONG=4 };
struct httpd_uri_t { const char* uri; int method; httpd_uri_func_t handler; void* user_ctx; bool is_websocket=false; };
struct httpd_ws_frame_t { int type{}; std::uint8_t* payload{}; std::size_t len{}; };
struct httpd_config_t { int max_uri_handlers; int stack_size; int max_open_sockets; bool lru_purge_enable; };
inline httpd_config_t HTTPD_DEFAULT_CONFIG(){ return {8,4096,7,false}; }
inline esp_err_t httpd_start(httpd_handle_t* h,const httpd_config_t*){*h=reinterpret_cast<void*>(1);return ESP_OK;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*){return ESP_OK;}
inline esp_err_t httpd_resp_set_status(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t*,const char*,std::ptrdiff_t){return ESP_OK;}
inline esp_err_t httpd_resp_send_err(httpd_req_t*,int,const char*){return ESP_OK;}
inline int httpd_req_to_sockfd(httpd_req_t*){ return 1; }
inline esp_err_t httpd_ws_send_frame_async(httpd_handle_t,int,httpd_ws_frame_t*){ return ESP_OK; }
inline esp_err_t httpd_ws_send_frame(httpd_req_t*,httpd_ws_frame_t*){ return ESP_OK; }
inline esp_err_t httpd_ws_recv_frame(httpd_req_t*,httpd_ws_frame_t*,std::size_t){ return ESP_OK; }
inline esp_err_t httpd_queue_work(httpd_handle_t,httpd_work_fn_t fn,void* arg){ if(fn) fn(arg); return ESP_OK; }
inline std::size_t httpd_req_get_hdr_value_len(httpd_req_t* req,const char* name){ if(req==nullptr || name==nullptr || req->mock_authorization==nullptr || std::strcmp(name,"Authorization")!=0) return 0; return std::strlen(req->mock_authorization); }
inline esp_err_t httpd_req_get_hdr_value_str(httpd_req_t* req,const char* name,char* buffer,std::size_t length){ if(req==nullptr || name==nullptr || buffer==nullptr || req->mock_authorization==nullptr || std::strcmp(name,"Authorization")!=0) return ESP_FAIL; const auto n=std::strlen(req->mock_authorization); if(n+1U>length) return ESP_FAIL; std::memcpy(buffer,req->mock_authorization,n+1U); return ESP_OK; }
inline int httpd_req_recv(httpd_req_t* req, char* buffer, std::size_t length){ if(req==nullptr || buffer==nullptr || req->mock_body==nullptr) return -1; const auto n=req->content_len<length?req->content_len:length; std::memcpy(buffer,req->mock_body,n); return static_cast<int>(n); }
