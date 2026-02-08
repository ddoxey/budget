#include "theme_config.h"

#include <filesystem>
#include <iostream>

#include "../io/json_kv.h"
#include "../io/paths.h"

namespace budget::ui {

namespace {

int parse_int(const std::string& text, int fallback) {
  if (text.empty()) {
    return fallback;
  }
  int value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return fallback;
    }
    value = value * 10 + (c - '0');
  }
  return value;
}

ThemeConfig default_config() {
  ThemeConfig cfg;
  cfg.title.fg = 1;
  cfg.title.bg = 148;
  cfg.title.style = "bold";
  cfg.header_style = "bold";
  cfg.banner.style = "bold";
  cfg.row_even.fg = 0;
  cfg.row_even.bg = 230;
  cfg.row_even.style = "bold";
  cfg.row_odd.fg = 0;
  cfg.row_odd.bg = 148;
  cfg.row_odd.style = "bold";
  return cfg;
}

}  // namespace

namespace {

ThemeConfig load_theme_config() {
  ThemeConfig out = default_config();
  std::string dir = budget::io::config_dir();
  if (dir.empty()) {
    return out;
  }
  std::filesystem::path path = std::filesystem::path(dir) / "budget_theme.json";
  bool exists = std::filesystem::exists(path);
  auto map = budget::io::read_json_kv(path.string());
  if (!map.has_value()) {
    if (exists) {
      std::cerr << "Warning: Unable to parse budget_theme.json at "
                << path.string() << ". Using defaults." << std::endl;
      return out;
    }
    budget::io::JsonValue defaults;
    defaults["title_fg"] = "1";
    defaults["title_bg"] = "148";
    defaults["title_style"] = "bold";
    defaults["header_style"] = "bold";
    defaults["header_fg"] = "232";
    defaults["header_bg"] = "231";
    defaults["banner_style"] = "bold";
    defaults["banner_fg"] = "";
    defaults["banner_bg"] = "";
    defaults["row_even_fg"] = "0";
    defaults["row_even_bg"] = "230";
    defaults["row_even_style"] = "bold";
    defaults["row_odd_fg"] = "0";
    defaults["row_odd_bg"] = "148";
    defaults["row_odd_style"] = "bold";
    std::filesystem::create_directories(path.parent_path());
    budget::io::write_json_kv(path.string(), defaults);
    return out;
  }
  if (map->count("title_fg")) {
    out.title.fg = parse_int(map->at("title_fg"), 1);
  }
  if (map->count("title_bg")) {
    out.title.bg = parse_int(map->at("title_bg"), 148);
  }
  if (map->count("title_style")) {
    out.title.style = map->at("title_style");
  }
  if (map->count("header_style")) {
    out.header_style = map->at("header_style");
  }
  if (map->count("header_fg")) {
    int fg = parse_int(map->at("header_fg"), -1);
    if (fg >= 0) {
      out.header_fg = fg;
    }
  }
  if (map->count("header_bg")) {
    int bg = parse_int(map->at("header_bg"), -1);
    if (bg >= 0) {
      out.header_bg = bg;
    }
  }
  if (map->count("banner_style")) {
    out.banner.style = map->at("banner_style");
  }
  if (map->count("banner_fg")) {
    int fg = parse_int(map->at("banner_fg"), -1);
    if (fg >= 0) {
      out.banner.fg = fg;
    }
  }
  if (map->count("banner_bg")) {
    int bg = parse_int(map->at("banner_bg"), -1);
    if (bg >= 0) {
      out.banner.bg = bg;
    }
  }
  if (map->count("row_even_fg")) {
    out.row_even.fg = parse_int(map->at("row_even_fg"), 0);
  }
  if (map->count("row_even_bg")) {
    out.row_even.bg = parse_int(map->at("row_even_bg"), 230);
  }
  if (map->count("row_even_style")) {
    out.row_even.style = map->at("row_even_style");
  }
  if (map->count("row_odd_fg")) {
    out.row_odd.fg = parse_int(map->at("row_odd_fg"), 0);
  }
  if (map->count("row_odd_bg")) {
    out.row_odd.bg = parse_int(map->at("row_odd_bg"), 15);
  }
  if (map->count("row_odd_style")) {
    out.row_odd.style = map->at("row_odd_style");
  }
  return out;
}

}  // namespace

namespace {
ThemeConfig g_cfg;
bool g_cfg_loaded = false;
}  // namespace

const ThemeConfig& theme_config() {
  if (!g_cfg_loaded) {
    g_cfg = load_theme_config();
    g_cfg_loaded = true;
  }
  return g_cfg;
}

void refresh_theme_config() {
  g_cfg = load_theme_config();
  g_cfg_loaded = true;
}

}  // namespace budget::ui
