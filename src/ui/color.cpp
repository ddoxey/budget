#include "color.h"

#include <sstream>
#include <unordered_map>

namespace budget::ui {

namespace {

const std::unordered_map<std::string, int> kStyleCodes = {
    {"normal", 0},    {"bold", 1},    {"faint", 2},  {"italic", 3},
    {"underline", 4}, {"blink", 5},   {"blink2", 6}, {"negative", 7},
    {"concealed", 8}, {"crossed", 9},
};

}  // namespace

std::string apply_color(const std::string& text, const Theme& theme) {
  std::ostringstream out;
  bool has = false;

  if (theme.fg.has_value()) {
    out << (has ? ";" : "") << "38;5;" << *theme.fg;
    has = true;
  }
  if (theme.bg.has_value()) {
    out << (has ? ";" : "") << "48;5;" << *theme.bg;
    has = true;
  }
  if (!theme.style.empty()) {
    auto it = kStyleCodes.find(theme.style);
    if (it != kStyleCodes.end()) {
      out << (has ? ";" : "") << it->second;
      has = true;
    }
  }

  if (!has) {
    return text;
  }

  return "\x1b[" + out.str() + "m" + text + "\x1b[0m";
}

}  // namespace budget::ui
