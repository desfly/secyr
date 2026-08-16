#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace hg {

enum class ModelZoneType : std::uint8_t { Perimeter, Interior, Fire, Flood, Tamper };
enum class ModelZoneState : std::uint8_t { Normal, Open, Alarm, Fault, Tamper, Bypassed };
enum class ModelSensorType : std::uint8_t { Digital, Temperature, Pressure, Current, Voltage };
enum class ModelOutputType : std::uint8_t { Relay, Siren, Valve, Light };
enum class PartitionArmState : std::uint8_t { Disarmed, Stay, Away, Alarm };
enum class SystemEventType : std::uint8_t {
    ZoneOpen, ZoneClosed, Alarm, Tamper, SensorOffline, BatteryLow,
    OutputOn, OutputOff, Armed, Disarmed, ConfigChanged,
};

struct SystemEvent {
    SystemEventType type{SystemEventType::ConfigChanged};
    std::uint16_t source_id{};
    std::uint64_t timestamp_ms{};
    std::uint64_t sequence{};
    std::int32_t value{};
};
using SystemEventCallback = void (*)(const SystemEvent&, void* context);

class SystemEventBus {
public:
    static constexpr std::size_t queue_capacity = 32;
    static constexpr std::size_t subscriber_capacity = 8;
    bool subscribe(SystemEventCallback callback, void* context = nullptr);
    bool publish(SystemEvent event);
    bool dispatch_one();
    std::size_t dispatch_all();
    [[nodiscard]] std::size_t queued() const;
    [[nodiscard]] std::uint64_t published() const;
    [[nodiscard]] std::uint64_t dropped() const;
private:
    struct Subscriber { SystemEventCallback callback{}; void* context{}; };
    mutable std::mutex mutex_;
    std::array<SystemEvent, queue_capacity> queue_{};
    std::array<Subscriber, subscriber_capacity> subscribers_{};
    std::size_t queue_head_{};
    std::size_t queue_tail_{};
    std::size_t queue_size_{};
    std::size_t subscriber_count_{};
    std::uint64_t published_{};
    std::uint64_t dropped_{};
    std::uint64_t next_sequence_{1};
};

struct ZoneRecord {
    std::uint16_t id{};
    std::array<char, 24> name{};
    ModelZoneType type{ModelZoneType::Perimeter};
    ModelZoneState state{ModelZoneState::Normal};
    bool enabled{true};
    bool bypassed{};
    bool always_on{};
};
struct SensorRecord {
    std::uint16_t id{};
    ModelSensorType type{ModelSensorType::Digital};
    bool online{};
    std::uint8_t battery_percent{100};
    std::int16_t rssi_dbm{};
    std::uint64_t last_seen_ms{};
};
struct OutputRecord {
    std::uint16_t id{};
    ModelOutputType type{ModelOutputType::Relay};
    bool active{};
    // For a valve, active=true means OPEN and active=false means CLOSE. This
    // bit distinguishes a real CLOSE command from the harmless power-on
    // default false state so boot never moves a valve merely by synchronizing.
    bool commanded{};
    // Incremented for every explicit command, even if it repeats the same
    // direction. Physical actuator runtime consumes each revision exactly once.
    std::uint32_t command_revision{};
    std::uint32_t timeout_ms{};
};
struct PartitionRecord {
    std::uint16_t id{};
    PartitionArmState arm_state{PartitionArmState::Disarmed};
    std::array<std::uint16_t, 16> zone_ids{};
    std::size_t zone_count{};
};

class SystemModel {
public:
    static constexpr std::size_t max_zones = 16;
    static constexpr std::size_t max_sensors = 16;
    static constexpr std::size_t max_outputs = 16;
    static constexpr std::size_t max_partitions = 4;
    explicit SystemModel(SystemEventBus& bus) : bus_(bus) {}
    bool add_zone(std::uint16_t id, std::string_view name, ModelZoneType type, bool always_on = false);
    bool add_sensor(std::uint16_t id, ModelSensorType type);
    bool add_output(std::uint16_t id, ModelOutputType type);
    bool add_partition(std::uint16_t id);
    bool set_zone_state(std::uint16_t id, ModelZoneState state, std::uint64_t now_ms);
    bool set_output_active(std::uint16_t id, bool active, std::uint64_t now_ms);
    bool set_partition_arm(std::uint16_t id, PartitionArmState state, std::uint64_t now_ms);

    // Thread-safe copy views for all runtime/API readers. Never retain a raw
    // pointer into the model across a task boundary.
    [[nodiscard]] bool zone_snapshot(std::uint16_t id, ZoneRecord& out) const;
    [[nodiscard]] bool sensor_snapshot(std::uint16_t id, SensorRecord& out) const;
    [[nodiscard]] bool output_snapshot(std::uint16_t id, OutputRecord& out) const;
    [[nodiscard]] bool partition_snapshot(std::uint16_t id, PartitionRecord& out) const;
    [[nodiscard]] bool zone_at_snapshot(std::size_t index, ZoneRecord& out) const;
    [[nodiscard]] bool sensor_at_snapshot(std::size_t index, SensorRecord& out) const;
    [[nodiscard]] bool output_at_snapshot(std::size_t index, OutputRecord& out) const;
    [[nodiscard]] bool partition_at_snapshot(std::size_t index, PartitionRecord& out) const;
    [[nodiscard]] std::size_t zone_count() const;
    [[nodiscard]] std::size_t sensor_count() const;
    [[nodiscard]] std::size_t output_count() const;
    [[nodiscard]] std::size_t partition_count() const;

    // Legacy startup/test views. Runtime code must use the snapshot API above;
    // returned pointers are not protected after this call returns.
    [[nodiscard]] const ZoneRecord* zone(std::uint16_t id) const;
    [[nodiscard]] const SensorRecord* sensor(std::uint16_t id) const;
    [[nodiscard]] const OutputRecord* output(std::uint16_t id) const;
    [[nodiscard]] const PartitionRecord* partition(std::uint16_t id) const;
    [[nodiscard]] const ZoneRecord* zone_at(std::size_t index) const { return index < zone_count_ ? &zones_[index] : nullptr; }
    [[nodiscard]] const SensorRecord* sensor_at(std::size_t index) const { return index < sensor_count_ ? &sensors_[index] : nullptr; }
    [[nodiscard]] const OutputRecord* output_at(std::size_t index) const { return index < output_count_ ? &outputs_[index] : nullptr; }
    [[nodiscard]] const PartitionRecord* partition_at(std::size_t index) const { return index < partition_count_ ? &partitions_[index] : nullptr; }
private:
    static void copy_name(std::array<char, 24>& destination, std::string_view source);
    bool emit(SystemEventType type, std::uint16_t source_id, std::uint64_t now_ms, std::int32_t value = 0);
    SystemEventBus& bus_;
    mutable std::mutex mutex_;
    std::array<ZoneRecord, max_zones> zones_{};
    std::array<SensorRecord, max_sensors> sensors_{};
    std::array<OutputRecord, max_outputs> outputs_{};
    std::array<PartitionRecord, max_partitions> partitions_{};
    std::size_t zone_count_{};
    std::size_t sensor_count_{};
    std::size_t output_count_{};
    std::size_t partition_count_{};
};

}  // namespace hg
