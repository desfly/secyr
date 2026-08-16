#include "hg_factory_reset_runtime.hpp"

#include "hg_rgb_diagnostic.hpp"

namespace homeguard::idf {

FactoryResetReport perform_factory_reset_with_indicator() {
    // Cemented field requirement: every Factory Reset path visibly announces
    // the destructive operation using the field-proven onboard WS2812 on GPIO48.
    // The image/firmware identity is untouched; only mutable state is erased.
    (void)RgbDiagnostic::test_white(48, 5000U);
    return FactoryResetManager{}.erase_mutable_state();
}

}  // namespace homeguard::idf
