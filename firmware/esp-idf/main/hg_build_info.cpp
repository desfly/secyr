#include "hg_build_info.hpp"
#include "hg_version.hpp"

#include <sstream>

#ifndef HG_GIT_REVISION
#define HG_GIT_REVISION "unknown"
#endif

#ifndef HG_CI_BUILD_NUMBER
#define HG_CI_BUILD_NUMBER "local"
#endif

#ifndef HG_BUILD_TIMESTAMP_UTC
#define HG_BUILD_TIMESTAMP_UTC "unknown"
#endif

namespace homeguard::idf {

BuildInfo current_build_info()
{
    return {
        HG_PROJECT_NAME,
        HG_CI_BUILD_NUMBER,
        HG_FIRMWARE_VERSION,
        HG_BOARD_NAME,
        HG_MODULE_NAME,
        HG_ESP_IDF_REQUIRED,
        HG_GIT_REVISION,
        HG_BUILD_TIMESTAMP_UTC,
    };
}

std::string build_info_json(const BuildInfo& info)
{
    std::ostringstream output;
    output
        << "{"
        << "\"project\":\"" << info.project << "\","
        << "\"build\":\"" << info.build << "\","
        << "\"version\":\"" << info.version << "\","
        << "\"board\":\"" << info.board << "\","
        << "\"module\":\"" << info.module << "\","
        << "\"esp_idf_required\":\"" << info.esp_idf_required << "\","
        << "\"git_revision\":\"" << info.git_revision << "\","
        << "\"build_timestamp_utc\":\"" << info.build_timestamp_utc << "\""
        << "}";
    return output.str();
}

}  // namespace homeguard::idf
