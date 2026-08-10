#include "header_map.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "json_map.h"

namespace budget::io {

namespace {

std::string strip_bom(const std::string& s) {
  if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
      static_cast<unsigned char>(s[1]) == 0xBB &&
      static_cast<unsigned char>(s[2]) == 0xBF) {
    return s.substr(3);
  }
  return s;
}

std::string normalize_tokens(const std::string& header) {
  std::string out;
  bool prev_space = false;
  for (char c : header) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      out.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      prev_space = false;
    } else {
      if (!prev_space) {
        out.push_back(' ');
        prev_space = true;
      }
    }
  }
  while (!out.empty() && out.front() == ' ') {
    out.erase(out.begin());
  }
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

bool contains_token(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::string best_match(const std::vector<std::string>& headers,
                       const std::vector<std::string>& normalized,
                       const std::vector<std::string>& candidates) {
  int best_score = -1;
  std::string best;
  for (size_t i = 0; i < headers.size(); ++i) {
    int score = -1;
    for (const auto& candidate : candidates) {
      if (normalized[i] == candidate) {
        score = std::max(score, 100 + static_cast<int>(candidate.size()));
      } else if (contains_token(normalized[i], candidate)) {
        score = std::max(score, static_cast<int>(candidate.size()));
      }
    }
    if (score > best_score) {
      best_score = score;
      best = headers[i];
    }
  }
  return best;
}

std::unordered_map<std::string, std::string> guess_map(
    const std::vector<std::string>& headers) {
  auto normalized = normalize_headers(headers);
  std::unordered_map<std::string, std::string> result;

  result["transaction_date"] =
      best_match(headers, normalized,
                 {"transaction date", "trans date", "date", "posted date",
                  "posting date"});
  result["posting_date"] =
      best_match(headers, normalized, {"posting date", "posted date"});
  result["description"] = best_match(headers, normalized,
                                     {"description", "details", "memo",
                                      "transaction description", "narrative"});
  result["debit"] = best_match(headers, normalized,
                               {"amount", "debit", "withdrawal", "withdrawals",
                                "value", "transaction amount"});

  return result;
}

}  // namespace

HeaderMap load_or_guess_header_map(const std::vector<std::string>& headers,
                                   const std::string& config_path) {
  HeaderMap out;
  out.path = config_path;

  auto existing = read_json_map(config_path);
  if (existing.has_value()) {
    out.mapping = *existing;
    return out;
  }

  out.mapping = guess_map(headers);
  if (!config_path.empty()) {
    std::filesystem::create_directories(
        std::filesystem::path(config_path).parent_path());
    write_json_map(config_path, out.mapping);
  }
  return out;
}

std::unordered_map<std::string, std::string> apply_header_map(
    const std::unordered_map<std::string, std::string>& row,
    const HeaderMap& header_map) {
  auto result = row;
  for (const auto& pair : header_map.mapping) {
    const auto& internal_key = pair.first;
    const auto& source_key = pair.second;
    if (source_key.empty()) {
      continue;
    }
    auto it = row.find(normalize_header(source_key));
    if (it != row.end()) {
      result[internal_key] = it->second;
    }
  }
  return result;
}

std::vector<std::string> normalize_headers(
    const std::vector<std::string>& headers) {
  std::vector<std::string> result;
  result.reserve(headers.size());
  for (const auto& header : headers) {
    result.push_back(normalize_header(header));
  }
  return result;
}

std::string normalize_header(const std::string& header) {
  return normalize_tokens(strip_bom(header));
}

}  // namespace budget::io
