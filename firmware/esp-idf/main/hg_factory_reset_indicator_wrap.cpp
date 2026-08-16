#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"

#ifdef ESP_PLATFORM

// GNU ld --wrap redirects every call to the mangled C++ member symbol below.
// Define the wrapper with the exact global symbol name the linker expects.
extern "C" homeguard::idf::FactoryResetReport
__real__ZNK9homeguard3idf19FactoryResetManager19erase_mutable_stateEv(
    const homeguard::idf::FactoryResetManager* self);

extern "C" homeguard::idf::FactoryResetReport
__wrap__ZNK9homeguard3idf19FactoryResetManager19erase_mutable_stateEv(
    const homeguard::idf::FactoryResetManager* self)
{
    // Cemented behavior for every runtime Factory Reset path:
    // solid white on onboard WS2812/GPIO48 for five seconds, then erase.
    (void)homeguard::idf::RgbDiagnostic::test_white(48, 5000U);
    return __real__ZNK9homeguard3idf19FactoryResetManager19erase_mutable_stateEv(self);
}

#endif  // ESP_PLATFORM
