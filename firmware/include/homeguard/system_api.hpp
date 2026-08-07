#pragma once

#include "homeguard/system_model.hpp"
#include <string>
#include <string_view>

namespace hg {

[[nodiscard]] std::string_view system_event_type_name(SystemEventType type);
[[nodiscard]] std::string system_status_json(const SystemModel& model, const SystemEventBus& bus);
[[nodiscard]] std::string system_zones_json(const SystemModel& model);
[[nodiscard]] std::string system_outputs_json(const SystemModel& model);
[[nodiscard]] std::string system_partitions_json(const SystemModel& model);
[[nodiscard]] std::string system_event_json(const SystemEvent& event);

}  // namespace hg
