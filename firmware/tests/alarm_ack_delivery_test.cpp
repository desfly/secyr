#include "homeguard/alarm_ack_api.hpp"
#include "homeguard/alarm_ack_command.hpp"
#include "homeguard/alarm_ack_event.hpp"
#include "homeguard/alarm_ack_telemetry.hpp"
#include "homeguard/alarm_acknowledgement.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    AlarmAcknowledgementService service;
    AlarmAckCommandHandler handler(service);

    service.on_alarm_state(true, 501);

    const AlarmAckCommand command{
        501,
        "android:primary",
        "request-501",
        45000,
    };

    const auto response = handler.handle(command);
    assert(response.result == AlarmAckResult::Accepted);

    const auto audit = alarm_ack_audit_event(command, response);
    const auto audit_text = alarm_ack_audit_text(audit);
    assert(audit_text.find("result=accepted") != std::string::npos);
    assert(audit_text.find("request_id=request-501") != std::string::npos);

    const auto frame = alarm_ack_telemetry(response.state);
    const auto json = alarm_ack_telemetry_json(frame);
    assert(json.find("\"alarm_sequence\":\"501\"") != std::string::npos);
    assert(json.find("\"acknowledged\":true") != std::string::npos);
    assert(json.find("android:primary") != std::string::npos);

    const auto replay = handler.handle(command);
    assert(replay.replayed);

    const auto replay_event = alarm_ack_audit_event(command, replay);
    assert(alarm_ack_audit_text(replay_event).find("replayed=true") !=
           std::string::npos);

    std::cout << "alarm acknowledgement delivery tests PASS\n";
    return 0;
}
