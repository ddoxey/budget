#pragma once

#include <string>

namespace budget::io {

std::string config_dir();
std::string cache_dir();
std::string header_map_path();
std::string download_dir();

}  // namespace budget::io
