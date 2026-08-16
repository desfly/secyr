#pragma once

#include "homeguard/sha256.hpp"
#include "homeguard/telemetry.hpp"

#include <string>
#include <string_view>

namespace hg {

class BearerTokenVerifier {
public:
    BearerTokenVerifier() = default;
    explicit BearerTokenVerifier(std::string_view token) { reset(token); }
    void reset(std::string_view token);
    void clear();
    [[nodiscard]] bool configured() const { return configured_; }
    [[nodiscard]] bool authorized(std::string_view authorization_header) const;
private:
    Sha256Digest digest_{};
    bool configured_{};
};

[[nodiscard]] std::string telemetry_json(const TelemetryFrame& frame);

}  // namespace hg
