#include "esp_http_server.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/types.h>

namespace {

constexpr std::uint8_t kPngSignature[8]{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};
constexpr std::size_t kChunkSize = 4096U;

bool is_png_payload(const char* buffer, ssize_t length)
{
    return buffer != nullptr && length >= static_cast<ssize_t>(sizeof(kPngSignature)) &&
           std::memcmp(buffer, kPngSignature, sizeof(kPngSignature)) == 0;
}

}  // namespace

extern "C" esp_err_t __real_httpd_resp_set_type(httpd_req_t* request, const char* type);
extern "C" esp_err_t __real_httpd_resp_send(httpd_req_t* request, const char* buffer, ssize_t length);

extern "C" esp_err_t __wrap_httpd_resp_set_type(httpd_req_t* request, const char* type)
{
    // The approved Bruce asset is intentionally preserved byte-for-byte.  Its
    // content is PNG even though the historic route is /bruce.jpg, so correct
    // only the HTTP Content-Type instead of re-encoding or altering the image.
    if (type != nullptr && std::strcmp(type, "image/jpeg") == 0) {
        return __real_httpd_resp_set_type(request, "image/png");
    }
    return __real_httpd_resp_set_type(request, type);
}

extern "C" esp_err_t __wrap_httpd_resp_send(httpd_req_t* request, const char* buffer, ssize_t length)
{
    if (!is_png_payload(buffer, length)) {
        return __real_httpd_resp_send(request, buffer, length);
    }

    // The approved source is intentionally large.  Stream it in small chunks
    // so the ESP32-S3 HTTP task never has to push the entire portrait in one
    // send operation.  The payload bytes remain exactly unchanged.
    std::size_t offset = 0U;
    const auto total = static_cast<std::size_t>(length);
    while (offset < total) {
        const auto chunk = std::min(kChunkSize, total - offset);
        const auto error = httpd_resp_send_chunk(request, buffer + offset, chunk);
        if (error != ESP_OK) return error;
        offset += chunk;
    }
    return httpd_resp_send_chunk(request, nullptr, 0U);
}
