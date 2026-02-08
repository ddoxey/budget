#pragma once

#include <optional>
#include <string>

namespace budget::ui {

struct Theme {
  std::optional<int> fg;
  std::optional<int> bg;
  std::string style;  // "bold", "normal", etc.
};

std::string apply_color(const std::string& text, const Theme& theme);

}  // namespace budget::ui
