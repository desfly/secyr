#pragma once
#include "homeguard/types.hpp"
#include <cstdint>
namespace hg {
struct Command { uint64_t request_id{}; uint64_t issued_at_ms{}; CommandType type{CommandType::Disarm}; uint32_t challenge{}; bool authenticated{}; };
struct CommandResult { CommandCode code{CommandCode::Invalid}; bool executed{}; bool duplicate{}; };
bool dangerous(CommandType type);
}
