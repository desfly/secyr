#pragma once
#include "homeguard/controller.hpp"
#include "homeguard/local_api.hpp"
#include <cstdint>
#include <string>

struct httpd_req;
using httpd_req_t = httpd_req;

class RestServer {
public:
    bool begin(hg::Controller& controller, std::string_view local_api_token,
               std::string_view certificate_pem, std::string_view private_key_pem,
               std::string_view device_id, uint16_t port);
    void stop();
    [[nodiscard]] bool running() const { return server_ != nullptr; }
    [[nodiscard]] void* native_handle() const { return server_; }
private:
    static int status_entry(httpd_req_t* request);
    static int health_entry(httpd_req_t* request);
    static int challenge_entry(httpd_req_t* request);
    static int command_entry(httpd_req_t* request);
    int status(httpd_req_t* request);
    int health(httpd_req_t* request);
    int challenge(httpd_req_t* request);
    int command(httpd_req_t* request);
    bool authorize(httpd_req_t* request) const;
    void* server_{};
    hg::Controller* controller_{};
    hg::BearerTokenVerifier token_{};
    std::string certificate_pem_{};
    std::string private_key_pem_{};
    std::string device_id_{};
};
