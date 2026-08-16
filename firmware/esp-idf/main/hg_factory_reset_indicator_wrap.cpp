#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"

namespace homeguard::idf {
namespace {

// GNU ld --wrap redirects every ESP-IDF call to the real
// FactoryResetManager::erase_mutable_state() through this function.  The host
// test target does not enable the wrap, so the persistence primitive remains
// independently testable while all runtime entry points (hardware reset,
// Web UI and Android API) share one field-visible destructive-action signal.
FactoryResetReport real_erase(const FactoryResetManager* self)
    asm("__real__ZNK9homeguard3idf19FactoryResetManager19erase_mutable_stateEv");

FactoryResetReport wrapped_erase(const FactoryResetManager* self)
    asm("__wrap__ZNK9homeguard3idf19FactoryResetManager19erase_mutable_stateEv");

FactoryResetReport wrapped_erase(const FactoryResetManager* self)
{
    // Cemented behavior: solid white on the onboard WS2812/GPIO48 for five
    // seconds before mutable state is erased.  RgbDiagnostic turns the LED off
    // before returning, then the normal erase/reboot path continues.
    (void)RgbDiagnostic::test_white(48, 5000U);
    return real_erase(self);
}

}  // namespace
}  // namespace homeguard::idf
