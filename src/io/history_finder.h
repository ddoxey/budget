#pragma once

#include <optional>
#include <string>
#include <vector>

namespace budget::io {

std::optional<std::string> latest_csv_path();
std::vector<std::string> latest_csv_paths(size_t limit = 2);

}  // namespace budget::io
