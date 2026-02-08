#pragma once

#include <optional>
#include <string>

#include "color.h"

namespace budget::ui {

struct ThemeConfig {
  Theme title;
  std::optional<int> header_fg;
  std::optional<int> header_bg;
  std::string header_style;
  Theme banner;
  Theme row_even;
  Theme row_odd;
};

const ThemeConfig& theme_config();
void refresh_theme_config();

}  // namespace budget::ui
