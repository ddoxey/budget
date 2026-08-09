#include "history_finder.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "paths.h"

namespace budget::io {

std::vector<std::string> latest_csv_paths(size_t limit) {
  if (limit == 0) {
    return {};
  }
  const char* env = std::getenv("BUDGET_CSV");
  if (env && *env) {
    return {std::string(env)};
  }
  std::string dir = download_dir();
  if (dir.empty()) {
    return {};
  }
  std::filesystem::path download_dir = std::filesystem::path(dir);
  if (!std::filesystem::exists(download_dir)) {
    return {};
  }

  using Candidate =
      std::pair<std::filesystem::file_time_type, std::string>;
  std::vector<Candidate> candidates;

  for (const auto& entry : std::filesystem::directory_iterator(download_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto path = entry.path();
    if (path.extension() != ".csv") {
      continue;
    }
    candidates.emplace_back(entry.last_write_time(), path.string());
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.first != b.first) {
                return a.first > b.first;
              }
              return a.second > b.second;
            });

  std::vector<std::string> paths;
  paths.reserve(std::min(limit, candidates.size()));
  for (size_t i = 0; i < candidates.size() && i < limit; ++i) {
    paths.push_back(candidates[i].second);
  }
  return paths;
}

std::optional<std::string> latest_csv_path() {
  auto paths = latest_csv_paths(1);
  if (paths.empty()) {
    return std::nullopt;
  }
  return paths.front();
}

}  // namespace budget::io
