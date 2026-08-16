#pragma once

#include "hg_factory_reset.hpp"

namespace homeguard::idf {

// One physical Factory Reset sequence for every runtime entry point:
// Web/Android API and hardware reset sequence. The immutable firmware/hardware
// identity is preserved by FactoryResetManager; the onboard RGB provides the
// field-visible five-second destructive-action indication before NVS erasure.
FactoryResetReport perform_factory_reset_with_indicator();

}  // namespace homeguard::idf
