#pragma once

#include "hg_mcp23017.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class Mcp23017OutputBackend final : public hg::PhysicalOutputBackend {
public:
    void attach(Mcp23017* expander) noexcept;

    bool configure_output(int channel, bool initial_level) override;
    bool write_output(int channel, bool level) override;
    bool read_inputs(std::uint8_t* value) override;

private:
    static bool valid_channel(int channel) noexcept;
    static std::uint8_t interlocked_value(
        std::uint8_t current,
        int channel,
        bool level) noexcept;
    bool commit(std::uint8_t value);

    Mcp23017* expander_{};
    std::array<bool, 8> configured_{};
    std::uint8_t shadow_{};
};

}  // namespace homeguard::idf
