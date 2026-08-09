#include "test_framework.hpp"
#include "test_declarations.hpp"
#include <iostream>
int main(){test_zone();test_pressure();test_reliability();test_controller();test_connectivity();test_provisioning();test_build0009();test_build0013();test_build0032();test_build0037();test_build0038();test_build0045();test_build0046();test_build0047();test_build0048();test_build0049();test_build0050();test_build0051();test_build0052();test_build0053();test_build0054();test_build0055();test_access_privacy();test_self_profile();test_user_lifecycle();test_admin_user_directory();test_admin_user_commands();test_admin_pin_transport();test_admin_payload_auth();std::cout<<"HomeGuard-S3 access privacy: "<<testfw::checks<<" tests PASS\n";return 0;}
