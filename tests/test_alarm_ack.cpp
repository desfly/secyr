#include "test_framework.hpp"
#include "homeguard/alarm_ack_api.hpp"
#include "homeguard/alarm_ack_command.hpp"
#include "homeguard/alarm_acknowledgement.hpp"

#include <string>

void test_alarm_ack() {
    using namespace homeguard;

    AlarmAcknowledgementService service;
    CHECK(service.acknowledge(1, 100, "android") == AlarmAckResult::NoActiveAlarm);

    service.on_alarm_state(true, 42);
    CHECK(service.acknowledge(41, 110, "android") == AlarmAckResult::InvalidRequest);
    CHECK(service.acknowledge(42, 120, "android") == AlarmAckResult::Accepted);
    CHECK(service.acknowledge(42, 130, "web") == AlarmAckResult::AlreadyAcknowledged);

    service.on_alarm_state(false, 42);
    CHECK(!service.state().alarm_active);
    service.on_alarm_state(true, 43);
    CHECK(!service.state().acknowledged);
    CHECK(service.state().alarm_sequence == 43);

    AlarmAckCommandHandler handler(service);

    AlarmAckApiResult invalid = handle_alarm_ack_api(
        {"not-a-number", "android", "r1"}, 1000, handler);
    CHECK(invalid.http_status == 400);

    AlarmAckApiResult accepted = handle_alarm_ack_api(
        {"43", "android:phone", "r2"}, 1100, handler);
    CHECK(accepted.http_status == 200);
    CHECK(accepted.body.find("\"result\":\"accepted\"") != std::string::npos);
    CHECK(accepted.body.find("\"replayed\":false") != std::string::npos);

    AlarmAckApiResult replay = handle_alarm_ack_api(
        {"43", "android:phone", "r2"}, 1200, handler);
    CHECK(replay.http_status == 200);
    CHECK(replay.body.find("\"replayed\":true") != std::string::npos);

    AlarmAckApiResult duplicate = handle_alarm_ack_api(
        {"43", "web", "r3"}, 1300, handler);
    CHECK(duplicate.http_status == 200);
    CHECK(duplicate.body.find("already_acknowledged") != std::string::npos);

    service.on_alarm_state(false, 43);
    AlarmAckApiResult no_alarm = handle_alarm_ack_api(
        {"43", "android:phone", "r4"}, 1400, handler);
    CHECK(no_alarm.http_status == 409);
    CHECK(no_alarm.body.find("no_active_alarm") != std::string::npos);
}
