#include "hg_ble_remote_http.hpp"

#include "hg_ble_remote_nvs.hpp"
#include "hg_http_util.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/ble_remote.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {
BleRemoteHttp* self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<BleRemoteHttp*>(request->user_ctx);
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

std::string json_escape(std::string_view value)
{
    std::string out;
    out.reserve(value.size() + 8U);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if (ch >= 0x20U) out.push_back(static_cast<char>(ch)); break;
        }
    }
    return out;
}

bool parse_bool(const std::string& body, const char* key, bool& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4U, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5U, "false") == 0) { value = false; return true; }
    return false;
}

bool parse_identity(std::string_view text, std::array<std::uint8_t, 16>& identity)
{
    if (text.size() != identity.size() * 2U) return false;
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return -1;
    };
    bool nonzero = false;
    for (std::size_t index = 0; index < identity.size(); ++index) {
        const int high = nibble(text[index * 2U]);
        const int low = nibble(text[index * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        identity[index] = static_cast<std::uint8_t>((high << 4) | low);
        nonzero = nonzero || identity[index] != 0U;
    }
    return nonzero;
}

std::string identity_text(const std::array<std::uint8_t, 16>& identity)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string result(identity.size() * 2U, '0');
    for (std::size_t index = 0; index < identity.size(); ++index) {
        result[index * 2U] = hex[(identity[index] >> 4U) & 0x0fU];
        result[index * 2U + 1U] = hex[identity[index] & 0x0fU];
    }
    return result;
}

bool admin_request(httpd_req_t* request, homeguard::AccessControl& access, std::string_view actor)
{
    return !actor.empty() && request_auth::authenticated_actor(request, access, actor) &&
           access.authorize_session(actor, "access.manage") == homeguard::AuditDecision::Allowed;
}

bool permissions_fit_owner(const homeguard::AccessControl& access,
                           homeguard::AccessRole role,
                           const hg::BleRemotePermissions& permissions)
{
    if (permissions.arm_home && !access.role_allows(role, "security.arm_home")) return false;
    if (permissions.arm_away && !access.role_allows(role, "security.arm_away")) return false;
    if (permissions.disarm && !access.role_allows(role, "security.disarm")) return false;
    if (permissions.panic && !access.role_allows(role, "security.panic")) return false;
    if ((permissions.light || permissions.lock_pulse) && !access.role_allows(role, "output.control")) return false;
    return true;
}

std::string permissions_json(const hg::BleRemotePermissions& p)
{
    return std::string{"{\"armHome\":"} + (p.arm_home ? "true" : "false") +
        ",\"armAway\":" + (p.arm_away ? "true" : "false") +
        ",\"disarm\":" + (p.disarm ? "true" : "false") +
        ",\"light\":" + (p.light ? "true" : "false") +
        ",\"lockPulse\":" + (p.lock_pulse ? "true" : "false") +
        ",\"panic\":" + (p.panic ? "true" : "false") + "}";
}
}

esp_err_t BleRemoteHttp::register_handlers(httpd_handle_t server,
                                           hg::BleRemoteRegistry* registry,
                                           BleRemoteNvsStore* store,
                                           homeguard::AccessControl* access)
{
    if (server == nullptr || registry == nullptr || store == nullptr || access == nullptr) return ESP_ERR_INVALID_ARG;
    registry_ = registry;
    store_ = store;
    access_ = access;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/access/remotes", .method=HTTP_GET, .handler=&BleRemoteHttp::list_get, .user_ctx=this},
        {.uri="/api/v1/access/remotes", .method=HTTP_POST, .handler=&BleRemoteHttp::upsert_post, .user_ctx=this},
        {.uri="/api/v1/access/remotes", .method=HTTP_DELETE, .handler=&BleRemoteHttp::remove_delete, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t BleRemoteHttp::list_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_list(request);
}

esp_err_t BleRemoteHttp::upsert_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_upsert(request);
}

esp_err_t BleRemoteHttp::remove_delete(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_remove(request);
}

esp_err_t BleRemoteHttp::handle_list(httpd_req_t* request)
{
    if (registry_ == nullptr || access_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *access_)) return request_auth::send_login_required(request);

    std::string body = "{\"ok\":true,\"count\":" + std::to_string(registry_->count()) +
        ",\"capacity\":" + std::to_string(hg::BleRemoteRegistry::kMaxBindings) + ",\"users\":[";
    bool first = true;
    for (std::size_t index = 0; index < access_->user_count(); ++index) {
        const auto* user = access_->user_at(index);
        if (user == nullptr) continue;
        if (!first) body += ',';
        first = false;
        body += "{\"id\":\"" + json_escape(user->id.data()) + "\",\"name\":\"" + json_escape(user->name.data()) +
            "\",\"role\":\"" + std::string(homeguard::to_string(user->role)) + "\",\"enabled\":" +
            (user->enabled ? "true" : "false") + "}";
    }
    body += "],\"remotes\":[";
    first = true;
    for (std::size_t index = 0; index < hg::BleRemoteRegistry::kMaxBindings; ++index) {
        const auto* binding = registry_->binding_at(index);
        if (binding == nullptr) continue;
        if (!first) body += ',';
        first = false;
        const auto* owner = access_->find_user(binding->owner_user_id.data());
        body += "{\"identity\":\"" + identity_text(binding->identity) + "\",\"name\":\"" +
            json_escape(binding->name.data()) + "\",\"ownerUserId\":\"" + json_escape(binding->owner_user_id.data()) +
            "\",\"ownerName\":\"" + json_escape(owner == nullptr ? "" : owner->name.data()) +
            "\",\"ownerEnabled\":" + (owner != nullptr && owner->enabled ? "true" : "false") +
            ",\"permissions\":" + permissions_json(binding->permissions) + "}";
    }
    body += "]}";
    return send_json(request, body);
}

esp_err_t BleRemoteHttp::handle_upsert(httpd_req_t* request)
{
    if (registry_ == nullptr || store_ == nullptr || access_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!http_util::read_body(request, 1024U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor, identity_value, owner_user_id, name;
    if (!http_util::parse_json_string(body, "actor", actor) ||
        !http_util::parse_json_string(body, "identity", identity_value) ||
        !http_util::parse_json_string(body, "ownerUserId", owner_user_id) ||
        !http_util::parse_json_string(body, "name", name)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"missing_fields\"}");
    }
    if (!admin_request(request, *access_, actor)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, "{\"ok\":false,\"reason\":\"admin_required\"}");
    }

    std::array<std::uint8_t, 16> identity{};
    if (!parse_identity(identity_value, identity) || name.empty() || name.size() >= 32U ||
        owner_user_id.empty() || owner_user_id.size() >= 24U) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_remote\"}");
    }

    const auto* owner = access_->find_user(owner_user_id);
    if (owner == nullptr || !owner->enabled) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"owner_unavailable\"}");
    }

    hg::BleRemotePermissions permissions{};
    (void)parse_bool(body, "armHome", permissions.arm_home);
    (void)parse_bool(body, "armAway", permissions.arm_away);
    (void)parse_bool(body, "disarm", permissions.disarm);
    (void)parse_bool(body, "light", permissions.light);
    (void)parse_bool(body, "lockPulse", permissions.lock_pulse);
    (void)parse_bool(body, "panic", permissions.panic);
    if (!permissions_fit_owner(*access_, owner->role, permissions)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, "{\"ok\":false,\"reason\":\"permissions_exceed_owner_role\"}");
    }

    const hg::BleRemoteRegistry previous = *registry_;
    if (!registry_->bind(identity, owner_user_id, name, permissions)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"remote_capacity_or_identity\"}");
    }
    const auto save_error = store_->save(*registry_);
    if (save_error != ESP_OK) {
        *registry_ = previous;
        http_util::scrub(body);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"remote_persist_failed\"}");
    }
    http_util::scrub(body);
    return send_json(request, "{\"ok\":true,\"count\":" + std::to_string(registry_->count()) + "}");
}

esp_err_t BleRemoteHttp::handle_remove(httpd_req_t* request)
{
    if (registry_ == nullptr || store_ == nullptr || access_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!http_util::read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    std::string actor, identity_value;
    if (!http_util::parse_json_string(body, "actor", actor) ||
        !http_util::parse_json_string(body, "identity", identity_value) ||
        !admin_request(request, *access_, actor)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, "{\"ok\":false,\"reason\":\"admin_required\"}");
    }
    std::array<std::uint8_t, 16> identity{};
    if (!parse_identity(identity_value, identity)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_identity\"}");
    }

    const hg::BleRemoteRegistry previous = *registry_;
    if (!registry_->unbind(identity)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "404 Not Found");
        return send_json(request, "{\"ok\":false,\"reason\":\"remote_not_found\"}");
    }
    const auto save_error = store_->save(*registry_);
    if (save_error != ESP_OK) {
        *registry_ = previous;
        http_util::scrub(body);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"remote_persist_failed\"}");
    }
    http_util::scrub(body);
    return send_json(request, "{\"ok\":true,\"count\":" + std::to_string(registry_->count()) + "}");
}

}  // namespace homeguard::idf
