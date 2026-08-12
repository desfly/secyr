#include <cstdint>

extern "C" void app_main();

// ESP-IDF EMBED_TXTFILES/EMBED_FILES creates these linker symbols during the
// real firmware build. The host-link gate does not run the ESP-IDF embedding
// step, so provide harmless externally visible stand-ins that match
// hg_web_http.cpp exactly.
extern "C" {
extern const uint8_t _binary_index_html_start[] = {0};
extern const uint8_t _binary_index_html_end[] = {0};
extern const uint8_t _binary_app_css_start[] = {0};
extern const uint8_t _binary_app_css_end[] = {0};
extern const uint8_t _binary_app_js_start[] = {0};
extern const uint8_t _binary_app_js_end[] = {0};
extern const uint8_t _binary_bruce_jpg_start[] = {0};
extern const uint8_t _binary_bruce_jpg_end[] = {0};
}

int main()
{
    app_main();
    return 0;
}
