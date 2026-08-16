#pragma once

namespace homeguard::idf {

// Detects three short external RST/EN resets within a short boot window.
// Returns true after a factory-reset attempt has taken ownership of boot.
bool handle_triple_rst_factory_reset();

}  // namespace homeguard::idf
