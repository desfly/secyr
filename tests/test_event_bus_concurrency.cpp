#include "homeguard/system_model.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <future>
#include <chrono>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "Failed: " #condition << "\n"; std::abort(); } } while (false)

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
    REQUIRE(state.bus->dispatch_all() == 0);
}
void replenish(const hg::SystemEvent&, void* context) {
    auto& bus = *static_cast<hg::SystemEventBus*>(context);
    bus.publish({});
}
int main() {
    hg::SystemEventBus bus;
    State state{&bus};
    REQUIRE(bus.subscribe(receive, &state));
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
                REQUIRE(bus.queued() <= hg::SystemEventBus::queue_capacity);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    while (bus.dispatch_all() != 0) {}
    REQUIRE(bus.published() == 80000);
    REQUIRE(bus.queued() == 0);
    REQUIRE(state.ordered);
    REQUIRE(state.delivered + bus.dropped() == bus.published());

    // A disarm cannot be overwritten by a zone decision using an earlier
    // armed state: its write must wait until the compound decision completes.
    hg::SystemEventBus model_bus;
    hg::SystemModel model(model_bus);
    REQUIRE(model.add_partition(1));
    REQUIRE(model.set_partition_arm(1, hg::PartitionArmState::Away, 0));
    std::future<bool> disarm;
    std::promise<void> attempted;
    auto attempted_future = attempted.get_future();
    {
        const auto state_lock = model.lock();
        REQUIRE(model.partition(1)->arm_state == hg::PartitionArmState::Away);
        disarm = std::async(std::launch::async, [&] {
            attempted.set_value();
            return model.set_partition_arm(1, hg::PartitionArmState::Disarmed, 2);
        });
        attempted_future.wait();
        REQUIRE(disarm.wait_for(std::chrono::milliseconds(25)) == std::future_status::timeout);
        REQUIRE(model.set_partition_arm(1, hg::PartitionArmState::Alarm, 1));
    }
    REQUIRE(disarm.get());
    REQUIRE(model.partition_snapshot(1)->arm_state == hg::PartitionArmState::Disarmed);

    hg::SystemEventBus replenishing;
    REQUIRE(replenishing.subscribe(replenish, &replenishing));
    replenishing.publish({});
    REQUIRE(replenishing.dispatch_all() == hg::SystemEventBus::queue_capacity);
    REQUIRE(replenishing.queued() == 1);
}
