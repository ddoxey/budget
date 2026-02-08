#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace budget::io {

struct CsvDocument {
  std::vector<std::string> headers;
  std::vector<std::unordered_map<std::string, std::string>> rows;
};

CsvDocument read_csv(const std::string& path);

}  // namespace budget::io
