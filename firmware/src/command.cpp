#include "homeguard/command.hpp"
namespace hg { bool dangerous(CommandType t){return t==CommandType::OpenValves||t==CommandType::ResetAlarm||t==CommandType::ExitMaintenance;} }
