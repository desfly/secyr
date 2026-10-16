#include "hg_gpio_output_backend.hpp"
#include "homeguard/hardware_profile.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

namespace {
std::atomic<int> in_flight{0};
std::atomic<int> overlaps{0};
std::atomic<int> writes{0};
std::atomic<bool> fail_write{false};
std::uint8_t port = 0xA0;
}

// Fake I2C device, real shared output backend. No ESP or physical pins are used.
namespace homeguard::idf {
bool Mcp23017::ready() const noexcept { return true; }
esp_err_t Mcp23017::write_mirrored_outputs(std::uint8_t value) {
    if (in_flight.fetch_add(1) != 0) ++overlaps;
    ++writes;
    std::this_thread::yield();
    const bool failed = fail_write.exchange(false);
    if (!failed) port = (port & 0xFCU) | (value & 0x03U);
    --in_flight;
    return failed ? ESP_FAIL : ESP_OK;
}
esp_err_t Mcp23017::force_safe_outputs() { port = 0; return ESP_OK; }
}

int main() {
    using namespace homeguard::idf;
    GpioOutputBackend backend;
    Mcp23017 expander;
    backend.bind_expander(expander);
    assert(backend.configure_output(hg::direct_light_relay_gpio, false));
    assert(backend.configure_output(hg::direct_lock_relay_gpio, false));
    assert(!backend.write_output(-99, true));
    assert(!backend.write_output(41, true));
    assert(!backend.write_output(42, true));

    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    assert(model.add_output(4, hg::ModelOutputType::Light));
    assert(model.add_output(5, hg::ModelOutputType::Relay));
    assert(model.set_output_active(4, true, 0));
    assert(model.set_output_active(5, true, 0));
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&] {
        for (int n = 0; n < 100; ++n) assert(backend.synchronize_relays(model));
    });
    for (auto& worker : workers) worker.join();
    assert(overlaps == 0);
    assert(port == 0xA3);
    assert(backend.relay_levels() == 3);
    const int settled_writes = writes;
    assert(backend.synchronize_relays(model));
    assert(writes == settled_writes);

    // An automatic light transition must preserve an already active lock.
    assert(model.set_output_active(4, false, 0));
    assert(backend.synchronize_relays(model));
    assert(port == 0xA2);
    // An uncertain I2C write latches a fault; no caller may re-energize outputs.
    fail_write = true;
    assert(model.set_output_active(4, true, 0));
    assert(!backend.synchronize_relays(model));
    assert(port == 0);

    GpioOutputBackend shutdown;
    shutdown.bind_expander(expander);
    assert(shutdown.synchronize_relays(model));
    assert(port == 3);
    assert(shutdown.force_safe_relays());
    assert(port == 0);
    // Even an old ON model cannot reverse a global emergency safe-off.
    assert(!shutdown.synchronize_relays(model));
    assert(port == 0);
    assert(!backend.write_output(hg::direct_lock_relay_gpio, true));
    assert(!backend.synchronize_relays(model));
    assert(port == 0);
    return 0;
}
