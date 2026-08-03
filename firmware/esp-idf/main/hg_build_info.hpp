#pragma once

#include <string>

namespace homeguard::idf {

struct BuildInfo {
    std::string project;
    std::string build;
    std::string version;
    std::string board;
    std::string module;
    std::string esp_idf_required;
    std::string git_revision;
    std::string build_timestamp_utc;
};

BuildInfo current_build_info();
std::string build_info_json(const BuildInfo& info);

}  // namespace homeguard::idf
