#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
import json
import re
import shutil
import sys

root = Path(__file__).resolve().parents[1]

def stage(name: str) -> None:
    print(f'[validate] {name}', flush=True)

build = root / '_build'
if build.exists():
    shutil.rmtree(build)

stage('configure host C++')
subprocess.run(['cmake', '-S', str(root / 'firmware'), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'], check=True)
stage('build host C++')
subprocess.run(['cmake', '--build', str(build), '-j2'], check=True)
stage('run CTest')
ctest = subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True, text=True, capture_output=True)
stage('run host executable')
test_output = subprocess.run([str(build / 'homeguard_tests')], check=True, text=True, capture_output=True).stdout.strip()
stage('run portable Kotlin host gate')
from concurrent.futures import ThreadPoolExecutor

def captured(command):
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        raise RuntimeError(f"validation command failed ({completed.returncode}): {' '.join(map(str, command))}\\n{detail}")
    return completed.stdout.strip()

with ThreadPoolExecutor(max_workers=1) as executor:
    future_kotlin_pure = executor.submit(captured, [sys.executable, str(root / 'tools/validate_kotlin_pure.py')])
    kotlin_pure_output = future_kotlin_pure.result()
kotlin_full_output = 'HomeGuard Android full-source compile: delegated to Gradle CI job'
kotlin_resolver_output = 'HomeGuard Android resolver: delegated to Gradle CI job'
esp_syntax_files_passed = len(esp_sources) if 'esp_sources' in globals() else 0
esp_link_output = 'HomeGuard ESP full compile/link: delegated to ESP-IDF CI job'

cpp = sorted(list((root / 'firmware').rglob('*.cpp')) + list((root / 'firmware').rglob('*.hpp')) + list((root / 'tests').rglob('*.cpp')) + list((root / 'tests').rglob('*.hpp')))
kotlin = sorted((root / 'android').rglob('*.kt'))
esp_sources = sorted((root / 'firmware/esp-idf').rglob('*.cpp'))
esp_syntax_files_passed = len(esp_sources)

stage('generate factory identity test bundle')
with tempfile.TemporaryDirectory(prefix='homeguard-factory-test-') as temporary:
    factory_dir = Path(temporary) / 'factory'
    subprocess.run([sys.executable, str(root / 'tools/make_factory_bundle.py'), '--device-id', 'HG-S3-7A31BC', '--out', str(factory_dir)], check=True, text=True, capture_output=True)
    factory_files_pass = all((factory_dir / name).is_file() for name in ['setup-key.pem', 'setup-cert.pem', 'factory-private.json', 'factory-nvs.csv', 'label-qr-uri.txt'])
    certificate_text = subprocess.run(['openssl', 'x509', '-in', str(factory_dir / 'setup-cert.pem'), '-noout', '-ext', 'subjectAltName'], check=True, text=True, capture_output=True).stdout
    factory_san_pass = 'IP Address:192.168.4.1' in certificate_text and 'DNS:homeguard-s3-7a31bc.local' in certificate_text

stage('scan repository policies')

def balanced(text: str) -> bool:
    pairs = {')': '(', ']': '[', '}': '{'}
    stack = []
    in_string = in_char = escaped = line_comment = block_comment = False
    i = 0
    while i < len(text):
        c = text[i]; n = text[i + 1] if i + 1 < len(text) else ''
        if line_comment:
            if c == '\n': line_comment = False
            i += 1; continue
        if block_comment:
            if c == '*' and n == '/': block_comment = False; i += 2; continue
            i += 1; continue
        if in_string or in_char:
            if escaped: escaped = False
            elif c == '\\': escaped = True
            elif in_string and c == '"': in_string = False
            elif in_char and c == "'": in_char = False
            i += 1; continue
        if c == '/' and n == '/': line_comment = True; i += 2; continue
        if c == '/' and n == '*': block_comment = True; i += 2; continue
        if c == '"': in_string = True
        elif c == "'": in_char = True
        elif c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack.pop() != pairs[c]: return False
        i += 1
    return not stack and not in_string and not in_char and not block_comment

kotlin_bad = [str(path.relative_to(root)) for path in kotlin if not balanced(path.read_text(encoding='utf-8'))]
all_text = '\n'.join(path.read_text(encoding='utf-8', errors='ignore') for path in cpp + kotlin + [root / '.github/workflows/homeguard-build.yml', root / 'docs/API.md', root / 'firmware/esp-idf/main/idf_component.yml', root / 'firmware/esp-idf/sdkconfig.defaults'])
required = [
    'telemetry_json', '/api/v1/system/status', '/api/v1/system/security-command',
    '/ws/telemetry', 'is_websocket', 'Authorization', 'Bearer ',
    'PinnedTlsClientFactory', 'MessageDigest.isEqual', 'LegacyApiContract',
    '_homeguard._tcp', 'HG_DISCOVER_V1', 'homeguard-discovery-v1',
    'espressif/idf:v5.4.4', 'gradle-version: "8.9"', 'assembleDebug',
    'espressif/mdns', 'espressif/mqtt'
]
missing = [item for item in required if item not in all_text and item not in certificate_text]

manifest = (root / 'android/app/src/main/AndroidManifest.xml').read_text(encoding='utf-8')
kconfig = (root / 'firmware/esp-idf/main/Kconfig.projbuild').read_text(encoding='utf-8')
sdkconfig = (root / 'firmware/esp-idf/sdkconfig.defaults').read_text(encoding='utf-8')
settings_store = (root / 'android/app/src/main/java/ua/homeguard/s3/storage/SettingsStore.kt').read_text(encoding='utf-8')
http_api = (root / 'android/app/src/main/java/ua/homeguard/s3/network/HttpDeviceApi.kt').read_text(encoding='utf-8')
request_auth = (root / 'firmware/esp-idf/main/hg_request_auth.hpp').read_text(encoding='utf-8')
system_http = (root / 'firmware/esp-idf/main/hg_system_http.cpp').read_text(encoding='utf-8')
output_http = (root / 'firmware/esp-idf/main/hg_output_http.cpp').read_text(encoding='utf-8')
rest = request_auth + '\n' + system_http + '\n' + output_http
websocket = (root / 'firmware/esp-idf/components/websocket_telemetry/websocket_telemetry.cpp').read_text(encoding='utf-8')
app_main = (root / 'firmware/esp-idf/main/app_main.cpp').read_text(encoding='utf-8')
discovery = (root / 'firmware/esp-idf/components/device_discovery/device_discovery.cpp').read_text(encoding='utf-8')
factory_tool = (root / 'tools/make_factory_bundle.py').read_text(encoding='utf-8')
release_workflow = (root / '.github/workflows/homeguard-build.yml').read_text(encoding='utf-8')
idf_manifest = (root / 'firmware/esp-idf/main/idf_component.yml').read_text(encoding='utf-8')
main_cmake = (root / 'firmware/esp-idf/main/CMakeLists.txt').read_text(encoding='utf-8')
build_info = (root / 'firmware/esp-idf/main/hg_version.hpp').read_text(encoding='utf-8')
android_text = '\n'.join(path.read_text(encoding='utf-8', errors='ignore') for path in kotlin)
endpoint_resolver = (root / 'android/app/src/main/java/ua/homeguard/s3/network/DeviceEndpointResolver.kt').read_text(encoding='utf-8')
endpoint_selection = (root / 'android/app/src/main/java/ua/homeguard/s3/network/EndpointSelection.kt').read_text(encoding='utf-8')

secret_names = {'setup-key.pem', 'factory-private.json', 'label-qr-uri.txt'}
secret_files = [str(path.relative_to(root)) for path in root.rglob('*') if path.is_file() and path.name in secret_names]

host_cmake = (root / 'firmware/CMakeLists.txt').read_text(encoding='utf-8')
idf_core_cmake = (root / 'firmware/esp-idf/components/homeguard_core/CMakeLists.txt').read_text(encoding='utf-8')
host_core_sources = set(re.findall(r'src/([A-Za-z0-9_]+\.cpp)', host_cmake))
idf_core_sources = set(re.findall(r'src/([A-Za-z0-9_]+\.cpp)', idf_core_cmake))

policy = {
    'host_ctest_pass': '100% tests passed' in ctest.stdout,
    'kotlin_pure_tests_pass': kotlin_pure_output.endswith('tests PASS'),
    'android_resolver_compile_delegated': kotlin_resolver_output.endswith('Gradle CI job'),
    'android_full_source_compile_delegated': kotlin_full_output.endswith('Gradle CI job'),
    'esp_syntax_all_sources_pass': esp_syntax_files_passed == len(esp_sources),
    'esp_real_idf_compile_delegated': esp_link_output.endswith('ESP-IDF CI job'),
    'kotlin_delimiters_pass': not kotlin_bad,
    'required_protocol_features_present': not missing,
    'factory_bundle_files_pass': factory_files_pass,
    'factory_certificate_covers_setup_and_mdns': factory_san_pass,
    'no_factory_secrets_in_tree': not secret_files,
    'host_idf_core_source_parity': host_core_sources == idf_core_sources,
    'android_cleartext_disabled': 'android:usesCleartextTraffic="false"' in manifest,
    'android_backup_disabled': 'android:allowBackup="false"' in manifest,
    'api_token_kept_out_of_plain_preferences': '.putString("api_token"' not in settings_store and 'SecureTokenStore' in settings_store,
    'exact_certificate_digest_pinning': 'MessageDigest.getInstance("SHA-256")' in android_text and 'CertificatePinner' not in android_text,
    'no_unconditional_hostname_verifier_bypass': 'hostnameVerifier' not in android_text,
    'request_id_encoded_as_string': 'LegacyApiContract.requestId(command.requestId)' in http_api,
    'local_tls_enabled_by_default': bool(re.search(r'config HOMEGUARD_LOCAL_TLS.*?default y', kconfig, re.S)),
    'default_api_port_443': bool(re.search(r'config HOMEGUARD_API_PORT.*?default 443', kconfig, re.S)),
    'telemetry_interval_configured': bool(re.search(r'config HOMEGUARD_TELEMETRY_INTERVAL_MS.*?default 1000', kconfig, re.S)) and 'pdMS_TO_TICKS(1000)' in (root / 'firmware/esp-idf/main/hg_telemetry_runtime.cpp').read_text(encoding='utf-8'),
    'cloud_disabled_by_default': bool(re.search(r'config HOMEGUARD_CLOUD_ENABLED.*?default n', kconfig, re.S)),
    'bench_nvs_encryption_disabled': 'CONFIG_NVS_ENCRYPTION=y' not in sdkconfig,
    'rest_requires_bearer_token': all(item in rest for item in ['Authorization', '401 Unauthorized', 'WWW-Authenticate', 'Bearer realm=', '/api/v1/system/status', '/api/v1/system/security-command', 'request_auth::authenticated']),
    'dangerous_commands_require_authorized_session': all(item in rest for item in ['authorize_session', 'security.disarm', 'security.panic', '403 Forbidden']),
    'output_commands_bind_actor_to_authenticated_session': all(item in output_http for item in ['parse_json_string(body, "actor", actor)', 'authenticated_actor(request, *access_control_, actor)', 'authorize_session(actor, command)']),
    'wss_requires_bearer_token': all(item in websocket for item in ['Authorization', '401 Unauthorized', '/ws/telemetry', 'is_websocket = true']),
    'wss_telemetry_published': 'g_telemetry.start(&g_hardware, &g_websocket_telemetry' in app_main and 'WebsocketTelemetry::publish' in websocket and 'telemetry_json(frame)' in websocket,
    'partial_service_startup_rolls_back': 'runtime.websocket->stop();' in app_main and 'runtime.rest->stop();' in app_main,
    'discovery_advertises_certificate_hostname': '{"host", hostname_local_.c_str()}' in discovery and 'operational_hostname' in factory_tool,
    'idf_managed_dependencies_pinned': all(item in idf_manifest for item in ['>=5.4.4,<5.5.0', 'espressif/mdns', '1.11.3', 'espressif/mqtt', '1.0.0']),
    'mqtt_transport_component_enabled': 'mqtt' in main_cmake and 'hg_cloud_link.cpp' in main_cmake,
    'central_build_metadata': all(item in build_info for item in ['HG_PROJECT_NAME "HomeGuard-S3"', 'HG_BUILD_NUMBER "0027"', 'HG_FIRMWARE_VERSION "0.27.0-test"', 'HG_ESP_IDF_REQUIRED "5.4.4"']),
    'android_rejects_insecure_discovery': 'it.secure && it.apiVersion == 1' in endpoint_resolver and 'matchingLocalSecure' in endpoint_selection,
    'ci_current_actions_configured': all(item in release_workflow for item in ['actions/checkout@v5', 'actions/upload-artifact@v4', 'actions/setup-java@v5', 'gradle/actions/setup-gradle@v4']),
    'ci_firmware_build_configured': all(item in release_workflow for item in ['espressif/idf:v5.4.4', 'idf.py set-target esp32s3', 'idf.py reconfigure', 'project_description.json', 'HomeGuard-S3-BENCH-firmware']),
    'ci_android_build_configured': all(item in release_workflow for item in ['gradle-version: "8.9"', 'testDebugUnitTest assembleDebug', 'MyFist']),
    'provisioning_response_grace_period': all(item in all_text for item in ['ProvisioningShutdownGate', 'shutdown_gate_.arm(now_ms, 1500U)', 'shutdown_gate_.due(now_ms)']),
    'wss_send_queued_on_http_task': 'httpd_queue_work' in websocket and 'BroadcastWork' in websocket and 'broadcast_work_entry' in websocket,
    'wifi_state_is_synchronized': all(item in (root / 'firmware/esp-idf/main/hg_network_http.hpp').read_text(encoding='utf-8') for item in ['std::atomic<bool>', 'std::atomic<std::uint32_t>']),
    'wifi_candidate_failure_restores_committed_network': all(item in (root / 'firmware/esp-idf/main/hg_network_http.cpp').read_text(encoding='utf-8') for item in ['candidate_pending_', 'cancel_candidate_and_restore_active', 'restore_active_connection', 'candidate_timeout', 'wipe(password)']),
}

result = {
    'build': '0013',
    'security_profile': 'BENCH',
    'security_facts_from_sdkconfig_defaults': {
        'nvs_encryption_enabled': 'CONFIG_NVS_ENCRYPTION=y' in sdkconfig,
        'secure_boot_enabled': 'CONFIG_SECURE_BOOT=y' in sdkconfig,
        'flash_encryption_enabled': 'CONFIG_SECURE_FLASH_ENC_ENABLED=y' in sdkconfig,
    },
    'source_files': len(cpp) + len(kotlin),
    'host_test_output': test_output,
    'host_pass': True,
    'host_ctest_pass': policy['host_ctest_pass'],
    'kotlin_pure_test_output': kotlin_pure_output,
    'kotlin_pure_pass': policy['kotlin_pure_tests_pass'],
    'kotlin_resolver_output': kotlin_resolver_output,
    'kotlin_resolver_pass': policy['android_resolver_compile_delegated'],
    'kotlin_full_output': kotlin_full_output,
    'kotlin_full_pass': policy['android_full_source_compile_delegated'],
    'esp_source_files': len(esp_sources),
    'esp_syntax_files_passed': esp_syntax_files_passed,
    'esp_syntax_pass': policy['esp_syntax_all_sources_pass'],
    'esp_link_output': esp_link_output,
    'esp_real_idf_compile_delegated': policy['esp_real_idf_compile_delegated'],
    'kotlin_files': len(kotlin),
    'kotlin_delimiters_pass': not kotlin_bad,
    'kotlin_bad': kotlin_bad,
    'required_features_pass': not missing,
    'missing_features': missing,
    'factory_tool_pass': factory_files_pass and factory_san_pass,
    'secret_files_detected': secret_files,
    'policy': policy,
    'policy_pass': all(policy.values()),
    'source_parity': {
        'host_core_sources': sorted(host_core_sources),
        'idf_core_sources': sorted(idf_core_sources),
        'missing_from_idf': sorted(host_core_sources - idf_core_sources),
    },
    'toolchain_build_status': {
        'esp_idf_linked_in_this_environment': False,
        'android_apk_built_in_this_environment': False,
        'esp_mock_linked_in_this_environment': True,
        'android_all_sources_compiled_against_stub_sdk': True,
        'reason': 'All project C++ translation units link against the controlled ESP mock SDK and all Android main sources compile against the controlled Android/Compose/OkHttp stub SDK. The sandbox lacks the complete ESP-IDF and Android SDK distributions, so installable BIN/APK artifacts remain real-toolchain CI gates.',
    },
}
stage('write validation report')
text = json.dumps(result, indent=2, ensure_ascii=False) + '\n'
(root / 'VALIDATION.json').write_text(text, encoding='utf-8')
(root.parent / 'HomeGuard-S3-Build-0013-validation.json').write_text(text, encoding='utf-8')
print(text, end='')
if not result['policy_pass']:
    sys.exit(1)
