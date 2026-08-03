#include "hg_build_info.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard::idf;

int main()
{
    const auto info = current_build_info();
    assert(info.project == "HomeGuard-S3");
    assert(info.build == "0022");
    assert(info.version == "0.22.0");
    assert(info.board == "HW-678 V0.0.0");

    const auto json = build_info_json(info);
    assert(json.find("\"build\":\"0022\"") != std::string::npos);
    assert(json.find("ESP32-S3-WROOM-1-N16R8") != std::string::npos);

    std::cout << "Build-0022 build-info tests PASS\n";
    return 0;
}
