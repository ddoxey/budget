#include "csv_reader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "header_map.h"

namespace budget::io {

namespace {

std::vector<std::string> parse_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;
  bool in_quotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (in_quotes) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          current.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        current.push_back(c);
      }
    } else {
      if (c == '"') {
        in_quotes = true;
      } else if (c == ',') {
        fields.push_back(current);
        current.clear();
      } else {
        current.push_back(c);
      }
    }
  }
  fields.push_back(current);
  return fields;
}

}  // namespace

CsvDocument read_csv(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("Unable to open CSV: " + path);
  }

  CsvDocument doc;
  std::string line;
  if (!std::getline(in, line)) {
    return doc;
  }
  doc.headers = parse_csv_line(line);
  auto normalized = normalize_headers(doc.headers);

  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    auto fields = parse_csv_line(line);
    std::unordered_map<std::string, std::string> row;
    for (size_t i = 0; i < fields.size() && i < doc.headers.size(); ++i) {
      row[normalized[i]] = fields[i];
    }
    doc.rows.push_back(std::move(row));
  }

  return doc;
}

}  // namespace budget::io
