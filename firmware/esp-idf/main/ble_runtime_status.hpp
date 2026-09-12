#pragma once

namespace homeguard::idf {

struct BleRuntimeStatus {
    bool runtime_ready;
    bool link_connected;
    bool notifications_enabled;
};

}  // namespace homeguard::idf
