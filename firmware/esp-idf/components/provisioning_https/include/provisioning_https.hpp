#pragma once
#include "nvs_config_store.hpp"
#include "provisioning_service.hpp"
#include <cstdint>

struct httpd_req;
using httpd_req_t = httpd_req;

class ProvisioningHttpsServer {
public:
    bool begin(ProvisioningService& service, const FactoryProvisioningIdentity& identity, uint16_t port);
    void stop();
    [[nodiscard]] bool running() const { return server_ != nullptr; }
private:
    static int info_entry(httpd_req_t* request);
    static int authorize_entry(httpd_req_t* request);
    static int apply_entry(httpd_req_t* request);
    int info(httpd_req_t* request);
    int authorize(httpd_req_t* request);
    int apply(httpd_req_t* request);
    void* server_{nullptr};
    ProvisioningService* service_{nullptr};
    const FactoryProvisioningIdentity* identity_{nullptr};
};
