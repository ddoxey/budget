#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace budget::io {

using JsonValue = std::unordered_map<std::string, std::string>;

std::optional<JsonValue> read_json_kv(const std::string& path);
bool write_json_kv(const std::string& path, const JsonValue& map);

}  // namespace budget::io
