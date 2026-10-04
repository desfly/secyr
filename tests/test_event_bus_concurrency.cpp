#include "homeguard/system_model.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

struct State {
    hg::SystemEventBus* bus;
    std::uint64_t last{};
    std::uint64_t delivered{};
    bool ordered{true};
};
void receive(const hg::SystemEvent& event, void* context) {
    auto& state = *static_cast<State*>(context);
    state.ordered = state.ordered && event.sequence > state.last;
    state.last = event.sequence;
    ++state.delivered;
    // Reentrant dispatch must return immediately, without delivering twice.
    assert(state.bus->dispatch_all() == 0);
}
void replenish(const hg::SystemEvent&, void* context) {
    auto& bus = *static_cast<hg::SystemEventBus*>(context);
    bus.publish({});
}
int main() {
    hg::SystemEventBus bus;
    State state{&bus};
    assert(bus.subscribe(receive, &state));
    std::atomic<int> producers{4};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 20000; ++j) {
                bus.publish({});
                if (j % 7 == 0) bus.dispatch_all();
            }
            --producers;
        });
    }
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&] {
            while (producers.load() != 0) {
                bus.dispatch_one();
                assert(bus.queued() <= hg::SystemEventBus::queue_capacity);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    while (bus.dispatch_all() != 0) {}
    assert(bus.published() == 80000);
    assert(bus.queued() == 0);
    assert(state.ordered);
    assert(state.delivered + bus.dropped() == bus.published());

    hg::SystemEventBus replenishing;
    assert(replenishing.subscribe(replenish, &replenishing));
    replenishing.publish({});
    assert(replenishing.dispatch_all() == hg::SystemEventBus::queue_capacity);
    assert(replenishing.queued() == 1);
}
