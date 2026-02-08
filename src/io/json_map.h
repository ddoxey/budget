#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace budget::io {

using StringMap = std::unordered_map<std::string, std::string>;

std::optional<StringMap> read_json_map(const std::string& path);
bool write_json_map(const std::string& path, const StringMap& map);

}  // namespace budget::io
