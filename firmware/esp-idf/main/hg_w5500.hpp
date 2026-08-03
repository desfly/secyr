#pragma once

#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_err.h"
#include <cstdint>
#include "esp_event.h"

#include <string>

namespace homeguard::idf {

struct W5500Status {
    bool initialized{false};
    bool link_up{false};
    bool has_ip{false};
    std::string ipv4;
};

class W5500 {
public:
    esp_err_t initialize();
    esp_err_t start();
    esp_err_t stop();
    const W5500Status& status() const noexcept;

private:
    static void on_eth_event(
        void* context,
        esp_event_base_t base,
        std::int32_t id,
        void* data);

    static void on_ip_event(
        void* context,
        esp_event_base_t base,
        std::int32_t id,
        void* data);

    esp_eth_handle_t eth_{nullptr};
    esp_netif_t* netif_{nullptr};
    W5500Status status_{};
};

}  // namespace homeguard::idf
