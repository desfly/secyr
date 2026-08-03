#pragma once

#include "homeguard/hardware_profile.hpp"

// Build-0013 deliberately contains no numeric GPIO assignments. The supplied
// HW-678 drawing confirms the module and peripheral signal names, but not the
// ESP32-S3 GPIO numbers. Configure pins in ESP-IDF menuconfig only after the
// final PCB/schematic continuity review.
namespace hg::pins {
inline constexpr int unassigned = gpio_unassigned;
}
