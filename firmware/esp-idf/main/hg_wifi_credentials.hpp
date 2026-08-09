#pragma once

#include "esp_err.h"

#include <array>

namespace homeguard::idf {

struct WifiCredentials {
    std::array<char, 33> ssid{};
    std::array<char, 65> password{};

    [[nodiscard]] bool valid() const noexcept { return ssid[0] != '\0'; }
};

class WifiCredentialStore {
public:
    esp_err_t load(WifiCredentials& credentials) const;
    esp_err_t save(const WifiCredentials& credentials) const;
    esp_err_t erase() const;
};

}  // namespace homeguard::idf
