#pragma once

#include "homeguard/physical_output_runtime.hpp"

namespace homeguard::idf {

class GpioOutputBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int gpio, bool initial_level) override;
    bool write_output(int gpio, bool level) override;
};

}  // namespace homeguard::idf
