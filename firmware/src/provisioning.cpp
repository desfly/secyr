#include "homeguard/provisioning.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <utility>

namespace hg {
namespace {
bool valid_ascii(std::string_view value, size_t min_length, size_t max_length) {
    if (value.size() < min_length || value.size() > max_length) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c){ return c >= 0x20U && c <= 0x7eU; });
}
bool is_hex(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char c){ return std::isxdigit(c) != 0; });
}
bool valid_wifi_password(std::string_view value) {
    return (valid_ascii(value, 8, 63)) || (value.size() == 64 && is_hex(value));
}
bool valid_mqtts(std::string_view endpoint) {
    return endpoint.empty() || (endpoint.starts_with("mqtts://") && endpoint.size() <= 255U);
}
}

bool ProvisioningPayload::valid(const ProvisioningPolicy& policy) const {
    if (!valid_ascii(wifi_ssid, 1, 32) || !valid_wifi_password(wifi_password)) return false;
    if (!valid_mqtts(cloud_endpoint)) return false;
    if (cloud_endpoint.empty() != cloud_token.empty()) return false;
    if (!cloud_token.empty() && !valid_ascii(cloud_token, 32, 256)) return false;
    if (policy.require_cloud_claim && cloud_token.empty()) return false;
    if (!valid_ascii(local_api_token, 32, 128)) return false;
    return owner_label.empty() || valid_ascii(owner_label, 1, 64);
}

void ProvisioningPayload::clear_secrets() {
    std::fill(wifi_password.begin(), wifi_password.end(), '\0');
    std::fill(cloud_token.begin(), cloud_token.end(), '\0');
    std::fill(local_api_token.begin(), local_api_token.end(), '\0');
    wifi_password.clear(); cloud_token.clear(); local_api_token.clear();
}

ProvisioningSession::ProvisioningSession(ProvisioningPolicy policy) : policy_(policy) {}

ProvisioningCode ProvisioningSession::begin(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms) {
    if (state_ == ProvisioningState::Provisioned) return ProvisioningCode::AlreadyProvisioned;
    if (!valid_pairing_code(pairing_code) || !valid_sha256_hex(certificate_sha256) || policy_.max_attempts == 0 ||
        policy_.code_ttl_ms == 0 || policy_.session_ttl_ms < policy_.code_ttl_ms) {
        state_ = ProvisioningState::Fault;
        return ProvisioningCode::InvalidPayload;
    }
    if (pending_) pending_->clear_secrets();
    pending_.reset();
    code_digest_ = sha256(pairing_code);
    certificate_digest_ = sha256(certificate_sha256);
    started_at_ms_ = now_ms;
    expires_at_ms_ = now_ms + policy_.session_ttl_ms;
    failed_attempts_ = 0;
    state_ = ProvisioningState::SetupAp;
    return ProvisioningCode::Accepted;
}

bool ProvisioningSession::expired(uint64_t now_ms) const {
    return now_ms > expires_at_ms_ || now_ms > started_at_ms_ + policy_.code_ttl_ms;
}

void ProvisioningSession::lock() {
    state_ = ProvisioningState::Locked;
    if (pending_) pending_->clear_secrets();
    pending_.reset();
}

ProvisioningCode ProvisioningSession::authorize(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms) {
    if (state_ == ProvisioningState::Locked) return ProvisioningCode::LockedOut;
    if (state_ != ProvisioningState::SetupAp) return ProvisioningCode::InvalidState;
    if (expired(now_ms)) { lock(); return ProvisioningCode::Expired; }
    const bool valid = valid_pairing_code(pairing_code) && valid_sha256_hex(certificate_sha256) &&
        constant_time_equal(code_digest_, sha256(pairing_code)) &&
        constant_time_equal(certificate_digest_, sha256(certificate_sha256));
    if (!valid) {
        ++failed_attempts_;
        if (failed_attempts_ >= policy_.max_attempts) { lock(); return ProvisioningCode::LockedOut; }
        return ProvisioningCode::InvalidProof;
    }
    state_ = ProvisioningState::Authorized;
    return ProvisioningCode::Accepted;
}

ProvisioningCode ProvisioningSession::submit(ProvisioningPayload payload, uint64_t now_ms) {
    if (state_ != ProvisioningState::Authorized) return state_ == ProvisioningState::Locked ? ProvisioningCode::LockedOut : ProvisioningCode::InvalidState;
    if (now_ms > expires_at_ms_) { lock(); return ProvisioningCode::Expired; }
    if (!payload.valid(policy_)) { payload.clear_secrets(); return ProvisioningCode::InvalidPayload; }
    pending_ = std::move(payload);
    state_ = ProvisioningState::Applying;
    return ProvisioningCode::Accepted;
}

bool ProvisioningSession::expire_if_needed(uint64_t now_ms) {
    const bool active = state_ == ProvisioningState::SetupAp || state_ == ProvisioningState::Authorized || state_ == ProvisioningState::Applying;
    if (!active || now_ms <= expires_at_ms_) return false;
    lock();
    return true;
}

ProvisioningCode ProvisioningSession::commit(bool storage_ok, uint64_t now_ms) {
    if (state_ != ProvisioningState::Applying || !pending_) return ProvisioningCode::InvalidState;
    if (now_ms > expires_at_ms_) { lock(); return ProvisioningCode::Expired; }
    pending_->clear_secrets();
    pending_.reset();
    if (!storage_ok) { state_ = ProvisioningState::Fault; return ProvisioningCode::StorageFailure; }
    state_ = ProvisioningState::Provisioned;
    return ProvisioningCode::Accepted;
}

void ProvisioningSession::abort() {
    if (pending_) pending_->clear_secrets();
    pending_.reset();
    if (state_ != ProvisioningState::Provisioned) state_ = ProvisioningState::Factory;
    failed_attempts_ = 0;
    expires_at_ms_ = 0;
}

bool ProvisioningSession::factory_reset(bool physical_presence) {
    if (!physical_presence) return false;
    if (pending_) pending_->clear_secrets();
    pending_.reset();
    code_digest_ = {};
    certificate_digest_ = {};
    state_ = ProvisioningState::Factory;
    failed_attempts_ = 0;
    started_at_ms_ = 0;
    expires_at_ms_ = 0;
    return true;
}

ProvisioningStatus ProvisioningSession::status(uint64_t now_ms) const {
    const bool is_authorized = state_ == ProvisioningState::Authorized || state_ == ProvisioningState::Applying;
    return {state_, failed_attempts_, expires_at_ms_, is_authorized, pending_.has_value() && now_ms <= expires_at_ms_};
}


void ProvisioningShutdownGate::arm(uint64_t now_ms, uint32_t delay_ms) {
    due_at_ms_ = delay_ms == 0U ? now_ms : now_ms + static_cast<uint64_t>(delay_ms);
    if (due_at_ms_ == 0U) due_at_ms_ = 1U;
}

void ProvisioningShutdownGate::clear() { due_at_ms_ = 0U; }

bool ProvisioningShutdownGate::due(uint64_t now_ms) const {
    return due_at_ms_ != 0U && now_ms >= due_at_ms_;
}

bool valid_pairing_code(std::string_view code) {
    return code.size() == 8U && std::all_of(code.begin(), code.end(), [](unsigned char c){ return std::isdigit(c) != 0; });
}

bool valid_sha256_hex(std::string_view fingerprint) {
    return fingerprint.size() == 64U && is_hex(fingerprint);
}

std::string format_pairing_code(uint32_t random_value) {
    std::array<char, 9> code{};
    std::snprintf(code.data(), code.size(), "%08u", static_cast<unsigned>(random_value % 100000000U));
    return code.data();
}
}
