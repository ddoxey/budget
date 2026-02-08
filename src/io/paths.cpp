#include "paths.h"

#include <cstdlib>
#include <filesystem>

#include "json_map.h"

namespace budget::io {

namespace {

std::string get_env(const char* key) {
  const char* val = std::getenv(key);
  if (val == nullptr) {
    return {};
  }
  return std::string(val);
}

bool is_macos() {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

struct BudgetConfig {
  std::string cache_dir;
  std::string download_dir;
};

std::optional<BudgetConfig> load_config() {
  try {
    auto cwd = std::filesystem::current_path() / "budget.json";
    auto home = get_env("HOME");
    if (!home.empty()) {
      auto home_cfg = std::filesystem::path(home) / "budget.json";
      if (std::filesystem::exists(cwd)) {
        auto map = read_json_map(cwd.string());
        if (map.has_value()) {
          BudgetConfig cfg;
          cfg.cache_dir = map->count("cache_dir") ? map->at("cache_dir") : "";
          cfg.download_dir =
              map->count("download_dir") ? map->at("download_dir") : "";
          return cfg;
        }
      }
      if (std::filesystem::exists(home_cfg)) {
        auto map = read_json_map(home_cfg.string());
        if (map.has_value()) {
          BudgetConfig cfg;
          cfg.cache_dir = map->count("cache_dir") ? map->at("cache_dir") : "";
          cfg.download_dir =
              map->count("download_dir") ? map->at("download_dir") : "";
          return cfg;
        }
      }
    }
  } catch (...) {
    return std::nullopt;
  }
  return std::nullopt;
}

const BudgetConfig& config() {
  static BudgetConfig cfg = []() {
    BudgetConfig out;
    auto loaded = load_config();
    if (loaded.has_value()) {
      out = *loaded;
    }
    return out;
  }();
  return cfg;
}

}  // namespace

std::string config_dir() {
  std::string home = get_env("HOME");
  if (home.empty()) {
    return {};
  }
  if (is_macos()) {
    return home + "/Library/Application Support/budget";
  }
  std::string xdg = get_env("XDG_CONFIG_HOME");
  if (!xdg.empty()) {
    return xdg + "/budget";
  }
  return home + "/.config/budget";
}

std::string cache_dir() {
  auto cfg = config();
  if (!cfg.cache_dir.empty()) {
    return cfg.cache_dir;
  }
  std::string home = get_env("HOME");
  if (home.empty()) {
    return {};
  }
  if (is_macos()) {
    return home + "/Library/Caches/budget";
  }
  std::string xdg = get_env("XDG_CACHE_HOME");
  if (!xdg.empty()) {
    return xdg + "/budget";
  }
  return home + "/.cache/budget";
}

std::string header_map_path() {
  std::string cfg = config_dir();
  if (cfg.empty()) {
    return {};
  }
  return cfg + "/header_map.json";
}

std::string download_dir() {
  auto cfg = config();
  if (!cfg.download_dir.empty()) {
    return cfg.download_dir;
  }
  std::string home = get_env("HOME");
  if (home.empty()) {
    return {};
  }
  return home + "/Downloads";
}

}  // namespace budget::io
