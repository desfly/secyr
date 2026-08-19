#include "alarm_ack_api.hpp"
#include "alarm_ack_command.hpp"
#include "alarm_acknowledgement.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    AlarmAcknowledgementService service;
    AlarmAckCommandHandler handler(service);

    service.on_alarm_state(true, 100);

    AlarmAckApiResult invalid = handle_alarm_ack_api(
        {"not-a-number", "android", "r1"}, 1000, handler);
    assert(invalid.http_status == 400);

    AlarmAckApiResult accepted = handle_alarm_ack_api(
        {"100", "android:phone", "r2"}, 1100, handler);
    assert(accepted.http_status == 200);
    assert(accepted.body.find("\"result\":\"accepted\"") != std::string::npos);
    assert(accepted.body.find("\"replayed\":false") != std::string::npos);

    AlarmAckApiResult replay = handle_alarm_ack_api(
        {"100", "android:phone", "r2"}, 1200, handler);
    assert(replay.http_status == 200);
    assert(replay.body.find("\"replayed\":true") != std::string::npos);

    AlarmAckApiResult duplicate = handle_alarm_ack_api(
        {"100", "web", "r3"}, 1300, handler);
    assert(duplicate.http_status == 200);
    assert(duplicate.body.find("already_acknowledged") != std::string::npos);

    service.on_alarm_state(false, 100);

    AlarmAckApiResult no_alarm = handle_alarm_ack_api(
        {"100", "android:phone", "r4"}, 1400, handler);
    assert(no_alarm.http_status == 409);
    assert(no_alarm.body.find("no_active_alarm") != std::string::npos);

    std::cout << "alarm ack command/API tests PASS\n";
    return 0;
}
