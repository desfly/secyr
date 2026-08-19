#include "test_framework.hpp"
#include "test_declarations.hpp"

#include <iostream>

int main()
{
    test_zone();
    test_pressure();
    test_reliability();
    test_controller();
    test_connectivity();
    test_provisioning();
    test_reset_sequence();
    test_alarm_ack();
    test_access_control();
    test_access_permissions();
    test_output_safety();
    test_readiness();
    test_build0009();
    test_build0013();
    test_build0032();
    test_build0038();
    test_build0045();
    test_build0046();

    std::cout << "HomeGuard-S3 host tests: " << testfw::checks << " checks PASS\n";
    return 0;
}
