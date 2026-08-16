#include "hg_w5500.hpp"
#include "hg_board_hw678.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"

#include <cstdint>

namespace homeguard::idf {

esp_err_t W5500::initialize()
{
    if (status_.initialized) {
        return ESP_OK;
    }

    const auto isr_error = gpio_install_isr_service(0);
    if (isr_error != ESP_OK && isr_error != ESP_ERR_INVALID_STATE) {
        return isr_error;
    }

    const spi_bus_config_t bus_config{
        .mosi_io_num = board::kW5500Mosi,
        .miso_io_num = board::kW5500Miso,
        .sclk_io_num = board::kW5500Sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0,
    };

    auto error = spi_bus_initialize(
        SPI2_HOST,
        &bus_config,
        SPI_DMA_CH_AUTO);
    if (error == ESP_OK) {
        spi_bus_owned_ = true;
    } else if (error != ESP_ERR_INVALID_STATE) {
        return error;
    }

    spi_device_interface_config_t spi_config{
        .command_bits = 16,
        .address_bits = 8,
        .dummy_bits = 0,
        .mode = 0,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 20 * 1000 * 1000,
        .input_delay_ns = 0,
        .spics_io_num = board::kW5500Cs,
        .flags = 0,
        .queue_size = 20,
        .pre_cb = nullptr,
        .post_cb = nullptr,
    };

    eth_w5500_config_t w5500_config =
        ETH_W5500_DEFAULT_CONFIG(
            SPI2_HOST,
            &spi_config);
    w5500_config.int_gpio_num =
        board::kW5500Interrupt;
    w5500_config.poll_period_ms = 0;

    eth_mac_config_t mac_config =
        ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config =
        ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num =
        board::kW5500Reset;

    mac_ = esp_eth_mac_new_w5500(
        &w5500_config,
        &mac_config);
    phy_ = esp_eth_phy_new_w5500(
        &phy_config);

    if (mac_ == nullptr || phy_ == nullptr) {
        (void)deinitialize();
        return ESP_ERR_NO_MEM;
    }

    esp_eth_config_t eth_config =
        ETH_DEFAULT_CONFIG(mac_, phy_);
    error = esp_eth_driver_install(
        &eth_config,
        &eth_);
    if (error != ESP_OK) {
        (void)deinitialize();
        return error;
    }

    esp_netif_config_t netif_config =
        ESP_NETIF_DEFAULT_ETH();
    netif_ = esp_netif_new(&netif_config);
    if (netif_ == nullptr) {
        (void)deinitialize();
        return ESP_ERR_NO_MEM;
    }

    glue_ = esp_eth_new_netif_glue(eth_);
    if (glue_ == nullptr) {
        (void)deinitialize();
        return ESP_ERR_NO_MEM;
    }

    error = esp_netif_attach(netif_, glue_);
    if (error != ESP_OK) {
        (void)deinitialize();
        return error;
    }

    error = esp_event_handler_register(
        ETH_EVENT,
        ESP_EVENT_ANY_ID,
        &W5500::on_eth_event,
        this);
    if (error != ESP_OK) {
        (void)deinitialize();
        return error;
    }
    eth_event_registered_ = true;

    error = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        &W5500::on_ip_event,
        this);
    if (error != ESP_OK) {
        (void)deinitialize();
        return error;
    }
    ip_event_registered_ = true;

    status_.initialized = true;
    return ESP_OK;
}

esp_err_t W5500::start()
{
    if (!status_.initialized || eth_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const auto error = esp_eth_start(eth_);
    if (error != ESP_OK) {
        (void)deinitialize();
    }
    return error;
}

esp_err_t W5500::stop()
{
    if (eth_ == nullptr) {
        return ESP_OK;
    }
    return esp_eth_stop(eth_);
}

esp_err_t W5500::deinitialize()
{
    esp_err_t first_error = ESP_OK;
    auto capture = [&first_error](esp_err_t error) {
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE && first_error == ESP_OK) {
            first_error = error;
        }
    };

    if (eth_ != nullptr && status_.initialized) {
        capture(esp_eth_stop(eth_));
    }

    if (ip_event_registered_) {
        capture(esp_event_handler_unregister(
            IP_EVENT,
            IP_EVENT_ETH_GOT_IP,
            &W5500::on_ip_event));
        ip_event_registered_ = false;
    }
    if (eth_event_registered_) {
        capture(esp_event_handler_unregister(
            ETH_EVENT,
            ESP_EVENT_ANY_ID,
            &W5500::on_eth_event));
        eth_event_registered_ = false;
    }

    if (glue_ != nullptr) {
        capture(esp_eth_del_netif_glue(glue_));
        glue_ = nullptr;
    }
    if (netif_ != nullptr) {
        esp_netif_destroy(netif_);
        netif_ = nullptr;
    }
    if (eth_ != nullptr) {
        capture(esp_eth_driver_uninstall(eth_));
        eth_ = nullptr;
    }
    if (phy_ != nullptr) {
        capture(phy_->del(phy_));
        phy_ = nullptr;
    }
    if (mac_ != nullptr) {
        capture(mac_->del(mac_));
        mac_ = nullptr;
    }
    if (spi_bus_owned_) {
        capture(spi_bus_free(SPI2_HOST));
        spi_bus_owned_ = false;
    }

    status_ = {};
    return first_error;
}

void W5500::on_eth_event(
    void* context,
    esp_event_base_t,
    std::int32_t id,
    void*)
{
    auto* self = static_cast<W5500*>(context);
    if (self == nullptr) {
        return;
    }

    if (id == ETHERNET_EVENT_CONNECTED) {
        self->status_.link_up = true;
    } else if (id == ETHERNET_EVENT_DISCONNECTED) {
        self->status_.link_up = false;
        self->status_.has_ip = false;
        self->status_.ipv4.clear();
    }
}

void W5500::on_ip_event(
    void* context,
    esp_event_base_t,
    std::int32_t,
    void* data)
{
    auto* self = static_cast<W5500*>(context);
    auto* event =
        static_cast<ip_event_got_ip_t*>(data);

    if (self == nullptr || event == nullptr) {
        return;
    }

    char buffer[16]{};
    esp_ip4addr_ntoa(
        &event->ip_info.ip,
        buffer,
        sizeof(buffer));

    self->status_.has_ip = true;
    self->status_.ipv4 = buffer;
}

const W5500Status& W5500::status() const noexcept
{
    return status_;
}

}  // namespace homeguard::idf
