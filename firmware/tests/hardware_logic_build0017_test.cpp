#include "homeguard/pt506_filter.hpp"
#include "homeguard/supervised_loop.hpp"
#include "homeguard/temperature_statistics.hpp"
#include "homeguard/valve_machine.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace homeguard;

int main()
{
    SupervisedLoopFilter zone({}, 1.0F, 3);
    zone.update(1500.0F);
    zone.update(1500.0F);
    auto normal = zone.update(1500.0F);
    assert(normal.state == SupervisedLoopState::Normal);

    zone.update(2300.0F);
    zone.update(2300.0F);
    auto alarm = zone.update(2300.0F);
    assert(alarm.state == SupervisedLoopState::Alarm);

    Pt506Filter pressure({}, 1.0F);
    auto p0 = pressure.update(480.0F, 0);
    assert(p0.state == PressureLoopState::Ok);
    assert(std::fabs(p0.pressure_bar) < 0.001F);

    auto p5 = pressure.update(1440.0F, 1000);
    assert(std::fabs(p5.pressure_bar - 5.0F) < 0.001F);

    auto broken = pressure.update(300.0F, 2000);
    assert(broken.state == PressureLoopState::OpenLoop);

    ValveMachine valve(30);
    valve.command(ValveCommand::Open, 0);
    assert(valve.snapshot().state == ValveState::Opening);
    assert(valve.snapshot().outputs.drive_open);

    auto opened = valve.update(
        {.limit_open = true},
        5000);
    assert(opened.state == ValveState::Open);
    assert(!opened.outputs.drive_open);

    valve.command(ValveCommand::EmergencyClose, 6000);
    assert(valve.snapshot().emergency_latched);
    assert(
        valve.snapshot().state ==
        ValveState::EmergencyClosing);

    auto closed = valve.update(
        {.limit_closed = true},
        10000);
    assert(closed.state == ValveState::Closed);

    valve.command(ValveCommand::Open, 11000);
    assert(valve.snapshot().state == ValveState::Closed);

    valve.clear_emergency_latch();
    valve.command(ValveCommand::Open, 12000);
    assert(valve.snapshot().state == ValveState::Opening);

    auto jammed = valve.update(
        {.overcurrent = true},
        12500);
    assert(jammed.state == ValveState::Jammed);

    TemperatureStatisticsFilter temperatures(3);
    assert(
        temperatures.update(20.0F, 0, true).valid);
    temperatures.update(21.0F, 60000, true);
    const auto stats =
        temperatures.update(22.0F, 120000, true);
    assert(std::fabs(stats.average_c - 21.0F) < 0.001F);
    assert(std::fabs(stats.rate_c_per_minute - 1.0F) < 0.001F);
    assert(!temperatures.update(85.0F, 180000, true).valid);

    std::cout << "Build-0017 hardware logic tests PASS\n";
    return 0;
}
