#include "history_finder.h"

#include <cstdlib>
#include <filesystem>

#include "paths.h"

namespace budget::io {

std::optional<std::string> latest_csv_path() {
  const char* env = std::getenv("BUDGET_CSV");
  if (env && *env) {
    return std::string(env);
  }
  std::string dir = download_dir();
  if (dir.empty()) {
    return std::nullopt;
  }
  std::filesystem::path download_dir = std::filesystem::path(dir);
  if (!std::filesystem::exists(download_dir)) {
    return std::nullopt;
  }
  std::optional<std::string> best;
  std::filesystem::file_time_type best_time;

  for (const auto& entry : std::filesystem::directory_iterator(download_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto path = entry.path();
    if (path.extension() != ".csv") {
      continue;
    }
    auto ts = entry.last_write_time();
    if (!best.has_value() || ts > best_time) {
      best = path.string();
      best_time = ts;
    }
  }
  return best;
}

}  // namespace budget::io
