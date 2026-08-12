#include "hg_config_http.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {
constexpr std::size_t kMaxImportBytes = 32768;

ConfigHttp* self_from(httpd_req_t* request) { return request == nullptr ? nullptr : static_cast<ConfigHttp*>(request->user_ctx); }

template <std::size_t N>
std::string_view text_view(const std::array<char, N>& value) {
    std::size_t n = 0; while (n < value.size() && value[n] != '\0') ++n; return {value.data(), n};
}

template <std::size_t N>
void copy_text(std::array<char, N>& out, std::string_view value) {
    out.fill('\0'); const auto n = std::min(value.size(), out.size() - 1U); std::copy_n(value.begin(), n, out.begin());
}

bool read_header(httpd_req_t* request, const char* name, std::string& value) {
    const auto length = httpd_req_get_hdr_value_len(request, name);
    if (length == 0 || length > 128) return false;
    value.assign(length + 1U, '\0');
    if (httpd_req_get_hdr_value_str(request, name, value.data(), value.size()) != ESP_OK) return false;
    value.resize(length);
    return true;
}

esp_err_t json_error(httpd_req_t* request, const char* status, const std::string& reason) {
    httpd_resp_set_status(request, status); httpd_resp_set_type(request, "application/json");
    const std::string body = std::string{"{\"ok\":false,\"reason\":\""} + reason + "\"}";
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

ModelZoneType model_zone_type(ConfigZoneType type) {
    switch (type) {
        case ConfigZoneType::EntryExit: return ModelZoneType::EntryExit;
        case ConfigZoneType::Interior: return ModelZoneType::Interior;
        case ConfigZoneType::Instant: return ModelZoneType::Instant;
        case ConfigZoneType::Fire24h: return ModelZoneType::Fire;
        case ConfigZoneType::Flood24h: return ModelZoneType::Flood;
        case ConfigZoneType::Tamper24h: return ModelZoneType::Tamper;
        case ConfigZoneType::Panic24h: return ModelZoneType::Panic;
        default: return ModelZoneType::Perimeter;
    }
}

ConfigZoneType config_zone_type(ModelZoneType type) {
    switch (type) {
        case ModelZoneType::EntryExit: return ConfigZoneType::EntryExit;
        case ModelZoneType::Interior: return ConfigZoneType::Interior;
        case ModelZoneType::Instant: return ConfigZoneType::Instant;
        case ModelZoneType::Fire: return ConfigZoneType::Fire24h;
        case ModelZoneType::Flood: return ConfigZoneType::Flood24h;
        case ModelZoneType::Tamper: return ConfigZoneType::Tamper24h;
        case ModelZoneType::Panic: return ConfigZoneType::Panic24h;
        default: return ConfigZoneType::Perimeter;
    }
}

ModelOutputType model_output_type(ConfigOutputType type) {
    switch (type) {
        case ConfigOutputType::Siren: return ModelOutputType::Siren;
        case ConfigOutputType::Valve: return ModelOutputType::Valve;
        case ConfigOutputType::Light: return ModelOutputType::Light;
        default: return ModelOutputType::Relay;
    }
}

ConfigOutputType config_output_type(ModelOutputType type) {
    switch (type) {
        case ModelOutputType::Siren: return ConfigOutputType::Siren;
        case ModelOutputType::Valve: return ConfigOutputType::Valve;
        case ModelOutputType::Light: return ConfigOutputType::Light;
        default: return ConfigOutputType::Relay;
    }
}
}

esp_err_t ConfigHttp::initialize(hg::SystemModel* model, AccessControl* access) {
    if (model == nullptr || access == nullptr) return ESP_ERR_INVALID_ARG;
    model_ = model; access_ = access; seed_from_runtime();
    HomeGuardConfigDocument persisted{};
    const auto loaded = store_.load(persisted);
    if (loaded == ESP_OK && can_apply(persisted)) {
        if (!apply(persisted)) return ESP_FAIL;
        document_ = persisted;
    }
    return ESP_OK;
}

void ConfigHttp::seed_from_runtime() {
    document_ = {};
    if (model_ != nullptr) {
        document_.zone_count = std::min(model_->zone_count(), document_.zones.size());
        for (std::size_t i = 0; i < document_.zone_count; ++i) {
            const auto* source = model_->zone_at(i); if (source == nullptr) continue;
            auto& target = document_.zones[i]; target.id = source->id; copy_text(target.name, text_view(source->name));
            target.type = config_zone_type(source->type); target.enabled = source->enabled; target.bypassed = source->bypassed;
            target.entry_delay_sec = source->entry_delay_sec; target.exit_delay_sec = source->exit_delay_sec;
        }
        document_.output_count = std::min(model_->output_count(), document_.outputs.size());
        for (std::size_t i = 0; i < document_.output_count; ++i) {
            const auto* source = model_->output_at(i); if (source == nullptr) continue;
            auto& target = document_.outputs[i]; target.id = source->id; copy_text(target.name, text_view(source->name));
            if (text_view(target.name).empty()) copy_text(target.name, std::string{"Output "} + std::to_string(source->id));
            target.type = config_output_type(source->type); target.enabled = source->enabled; target.timeout_sec = source->timeout_ms / 1000U;
        }
    }
    if (access_ != nullptr) {
        document_.user_count = std::min(access_->user_count(), document_.users.size());
        for (std::size_t i = 0; i < document_.user_count; ++i) {
            const auto* source = access_->user_at(i); if (source == nullptr) continue;
            auto& target = document_.users[i]; copy_text(target.id, text_view(source->id)); copy_text(target.name, text_view(source->name));
            target.role = source->role; target.enabled = source->enabled;
            (void)document_.zone_access.ensure_user(text_view(source->id));
            (void)document_.output_access.ensure_user(text_view(source->id));
        }
    }
}

bool ConfigHttp::can_apply(const HomeGuardConfigDocument& candidate) const {
    if (model_ == nullptr || access_ == nullptr || !validate_config_document(candidate).ok()) return false;
    if (candidate.user_count != access_->user_count()) return false;
    for (std::size_t i = 0; i < candidate.zone_count; ++i) if (model_->zone(candidate.zones[i].id) == nullptr) return false;
    for (std::size_t i = 0; i < candidate.output_count; ++i) if (model_->output(candidate.outputs[i].id) == nullptr) return false;
    for (std::size_t i = 0; i < candidate.user_count; ++i) if (access_->find_user(text_view(candidate.users[i].id)) == nullptr) return false;
    return true;
}

bool ConfigHttp::apply(const HomeGuardConfigDocument& candidate) {
    if (!can_apply(candidate)) return false;
    for (std::size_t i = 0; i < candidate.zone_count; ++i) {
        const auto& z = candidate.zones[i];
        if (!model_->configure_zone(z.id, text_view(z.name), model_zone_type(z.type), z.enabled, z.bypassed,
                                    z.entry_delay_sec, z.exit_delay_sec)) return false;
    }
    for (std::size_t i = 0; i < candidate.output_count; ++i) {
        const auto& o = candidate.outputs[i];
        if (!model_->configure_output(o.id, text_view(o.name), model_output_type(o.type), o.enabled, o.timeout_sec * 1000U)) return false;
    }
    for (std::size_t i = 0; i < candidate.user_count; ++i) {
        const auto& cuser = candidate.users[i];
        const auto* existing = access_->find_user(text_view(cuser.id)); if (existing == nullptr) return false;
        AccessUser replacement = *existing;
        copy_text(replacement.name, text_view(cuser.name)); replacement.role = cuser.role; replacement.enabled = cuser.enabled;
        if (!access_->import_user(replacement)) return false;
    }
    return true;
}

bool ConfigHttp::authorize_admin(httpd_req_t* request, const char* command) {
    std::string actor, credential;
    if (!read_header(request, "X-HG-Actor", actor) || !read_header(request, "X-HG-Credential", credential)) return false;
    const auto decision = access_->authorize(actor, credential, command);
    std::fill(credential.begin(), credential.end(), '\0');
    return decision == AuditDecision::Allowed;
}

esp_err_t ConfigHttp::register_handlers(httpd_handle_t server) {
    if (server == nullptr || model_ == nullptr || access_ == nullptr) return ESP_ERR_INVALID_ARG;
    const httpd_uri_t routes[] = {
        {.uri="/api/v1/config/export", .method=HTTP_GET, .handler=&ConfigHttp::export_get, .user_ctx=this},
        {.uri="/api/v1/config/import", .method=HTTP_POST, .handler=&ConfigHttp::import_post, .user_ctx=this},
    };
    for (const auto& route : routes) { const auto error = httpd_register_uri_handler(server, &route); if (error != ESP_OK) return error; }
    return ESP_OK;
}

esp_err_t ConfigHttp::export_get(httpd_req_t* request) { auto* self=self_from(request); return self==nullptr?ESP_FAIL:self->handle_export(request); }
esp_err_t ConfigHttp::import_post(httpd_req_t* request) { auto* self=self_from(request); return self==nullptr?ESP_FAIL:self->handle_import(request); }

esp_err_t ConfigHttp::handle_export(httpd_req_t* request) {
    if (!authorize_admin(request, "config.export")) return json_error(request, "403 Forbidden", "admin_required");
    const auto validation = validate_config_document(document_);
    if (!validation.ok()) return json_error(request, "409 Conflict", std::string{"config_invalid:"} + to_string(validation.error));
    const std::string body = export_config_json(document_);
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    httpd_resp_set_hdr(request, "Content-Disposition", "attachment; filename=homeguard-s3-config.json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

esp_err_t ConfigHttp::handle_import(httpd_req_t* request) {
    if (!authorize_admin(request, "config.import")) return json_error(request, "403 Forbidden", "admin_required");
    if (request->content_len == 0 || request->content_len > kMaxImportBytes) return json_error(request, "413 Payload Too Large", "invalid_config_size");
    std::string body(request->content_len, '\0');
    std::size_t total = 0;
    while (total < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + total, body.size() - total);
        if (received <= 0) return json_error(request, "400 Bad Request", "read_failed");
        total += static_cast<std::size_t>(received);
    }
    HomeGuardConfigDocument candidate{};
    const auto imported = import_config_json(body, candidate);
    std::fill(body.begin(), body.end(), '\0');
    if (!imported.ok()) {
        std::string reason = to_string(imported.error);
        if (imported.error == ConfigImportError::ValidationFailed) reason += std::string{":"} + to_string(imported.validation.error);
        return json_error(request, "400 Bad Request", reason);
    }
    if (!can_apply(candidate)) return json_error(request, "409 Conflict", "runtime_resources_or_users_mismatch");
    const auto saved = store_.save(candidate);
    if (saved != ESP_OK) return json_error(request, "507 Insufficient Storage", "nvs_save_failed");
    if (!apply(candidate)) return json_error(request, "500 Internal Server Error", "runtime_apply_failed");
    document_ = candidate;
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, "{\"ok\":true,\"state\":\"applied\"}", -1);
}

}  // namespace homeguard::idf
