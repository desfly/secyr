#pragma once
#include <cstdint>
#include <string>

struct SetupApConfig {
    std::string ssid;
    std::string password;
    uint8_t channel{6};
    uint8_t max_clients{1};
};

class SetupAp {
public:
    bool begin(const SetupApConfig& config);
    void stop();
    [[nodiscard]] bool active() const { return active_; }
private:
    bool active_{false};
};
