#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace budget::io {

struct HeaderMap {
  std::unordered_map<std::string, std::string>
      mapping;  // internal -> source header key
  std::string path;
};

HeaderMap load_or_guess_header_map(const std::vector<std::string>& headers,
                                   const std::string& config_path);

std::unordered_map<std::string, std::string> apply_header_map(
    const std::unordered_map<std::string, std::string>& row,
    const HeaderMap& header_map);

std::vector<std::string> normalize_headers(
    const std::vector<std::string>& headers);
std::string normalize_header(const std::string& header);

}  // namespace budget::io
