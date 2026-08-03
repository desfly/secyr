#include "homeguard/modbus_request.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    const auto request =
        modbus_read_holding_registers(
            1,
            0,
            2);

    assert(request[0] == 1);
    assert(request[1] == 3);
    assert(request[4] == 0);
    assert(request[5] == 2);

    const auto crc =
        modbus_crc16_portable(
            request.data(),
            6);

    assert(request[6] ==
           static_cast<std::uint8_t>(crc & 0xFF));
    assert(request[7] ==
           static_cast<std::uint8_t>(crc >> 8));

    std::cout << "Build-0020 runtime tests PASS\n";
    return 0;
}
