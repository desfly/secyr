#include "provisioning_service.hpp"
#include <algorithm>
#include <utility>

ProvisioningService::ProvisioningService(const hg::DeviceIdentity& identity, NvsConfigStore& store, SetupAp& setup_ap)
    : identity_(identity), store_(store), setup_ap_(setup_ap),
      session_({CONFIG_HOMEGUARD_PAIRING_CODE_TTL_SECONDS * 1000U,
                CONFIG_HOMEGUARD_SETUP_AP_TIMEOUT_SECONDS * 1000U,
                CONFIG_HOMEGUARD_PAIRING_MAX_ATTEMPTS,
                false}),
      device_id_(identity.device_id()) {}

bool ProvisioningService::begin(const ProvisioningBootstrap& bootstrap, uint64_t now_ms) {
    if (store_.is_provisioned() || !hg::valid_pairing_code(bootstrap.pairing_code) ||
        !hg::valid_sha256_hex(bootstrap.certificate_sha256) ||
        bootstrap.setup_password.size() < 12U || bootstrap.setup_password.size() > 63U ||
        !bootstrap.setup_url.starts_with("https://")) return false;
    const std::string setup_ssid = device_id_ + "-Setup";
    if (session_.begin(bootstrap.pairing_code, bootstrap.certificate_sha256, now_ms) != hg::ProvisioningCode::Accepted) return false;
    if (!setup_ap_.begin({setup_ssid, bootstrap.setup_password, bootstrap.channel, 1})) {
        session_.abort();
        return false;
    }
    qr_uri_ = hg::make_provisioning_uri({device_id_, setup_ssid, bootstrap.setup_url, bootstrap.setup_password,
                                         bootstrap.certificate_sha256, bootstrap.pairing_code});
    shutdown_gate_.clear();
    return true;
}

hg::ProvisioningCode ProvisioningService::authorize(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms) {
    return session_.authorize(pairing_code, certificate_sha256, now_ms);
}

hg::ProvisioningCode ProvisioningService::apply(hg::ProvisioningPayload payload, uint64_t now_ms) {
    const auto submitted = session_.submit(std::move(payload), now_ms);
    if (submitted != hg::ProvisioningCode::Accepted) return submitted;
    const bool stored = session_.pending().has_value() && store_.save_provisioning(*session_.pending());
    const auto committed = session_.commit(stored, now_ms);
    // Keep HTTPS/AP alive briefly so the /apply response reaches the phone before Wi-Fi stops.
    if (committed == hg::ProvisioningCode::Accepted) shutdown_gate_.arm(now_ms, 1500U);
    return committed;
}

bool ProvisioningService::factory_reset(bool physical_presence) {
    if (!physical_presence || !store_.erase_provisioning(true)) return false;
    return session_.factory_reset(true);
}

void ProvisioningService::tick(uint64_t now_ms) {
    if (shutdown_gate_.due(now_ms)) { stop(); return; }
    if (session_.expire_if_needed(now_ms)) stop();
}

void ProvisioningService::stop() {
    shutdown_gate_.clear();
    setup_ap_.stop();
    std::fill(qr_uri_.begin(), qr_uri_.end(), '\0');
    qr_uri_.clear();
}
