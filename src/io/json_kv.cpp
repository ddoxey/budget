#include "json_kv.h"

#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace budget::io {

namespace {

std::string strip_bom(const std::string& text) {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    return text.substr(3);
  }
  return text;
}

std::string remove_trailing_commas(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  bool in_string = false;
  bool escape = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (in_string) {
      out.push_back(c);
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
      out.push_back(c);
      continue;
    }
    if (c == ',') {
      size_t j = i + 1;
      while (j < text.size() &&
             std::isspace(static_cast<unsigned char>(text[j]))) {
        ++j;
      }
      if (j < text.size() && (text[j] == '}' || text[j] == ']')) {
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

}  // namespace

std::optional<JsonValue> read_json_kv(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return std::nullopt;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  std::string text = remove_trailing_commas(strip_bom(buf.str()));
  auto json = nlohmann::json::parse(text, nullptr, false, true);
  if (json.is_discarded() || !json.is_object()) {
    return std::nullopt;
  }
  JsonValue map;
  for (auto it = json.begin(); it != json.end(); ++it) {
    if (it.value().is_string()) {
      map[it.key()] = it.value().get<std::string>();
    } else if (it.value().is_number_integer()) {
      map[it.key()] = std::to_string(it.value().get<int>());
    } else if (it.value().is_number_float()) {
      map[it.key()] = std::to_string(it.value().get<double>());
    } else if (it.value().is_boolean()) {
      map[it.key()] = it.value().get<bool>() ? "true" : "false";
    }
  }
  return map;
}

bool write_json_kv(const std::string& path, const JsonValue& map) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& pair : map) {
    json[pair.first] = pair.second;
  }
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << json.dump(2) << "\n";
  return true;
}

}  // namespace budget::io
