#include "table.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "theme_config.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace budget::ui {

namespace {

std::string strip_ansi(const std::string& text) {
  std::string out;
  bool in_escape = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (!in_escape) {
      if (c == '\x1b') {
        in_escape = true;
      } else {
        out.push_back(c);
      }
    } else if (c == 'm') {
      in_escape = false;
    }
  }
  return out;
}

int display_width(const std::string& text) {
  return static_cast<int>(strip_ansi(text).size());
}

}  // namespace

Table::Table(std::vector<int> col_widths, bool use_color, BorderStyle style)
    : col_widths_(std::move(col_widths)), use_color_(use_color), style_(style) {
  themes_.assign(col_widths_.size(), Theme{});
  adjust_widths();
}

void Table::set_theme(const Theme& theme) {
  themes_.assign(col_widths_.size(), theme);
}

void Table::set_themes(const std::vector<Theme>& themes) {
  themes_ = themes;
  if (themes_.size() < col_widths_.size()) {
    themes_.resize(col_widths_.size());
  }
}

void Table::header(const std::vector<std::string>& cols,
                   const std::string& title) {
  if (!title.empty()) {
    ensure_title_fits(title);
    ThemeConfig cfg = theme_config();
    Theme title_theme = cfg.title;
    auto previous = themes_;
    if (style_ == BorderStyle::Unicode) {
      append_border("╔", "═", "═", "╗");
      set_theme(title_theme);
      append_title_row("║", "║", title, title_theme, true);
      append_border("╠", "═", "╦", "╣");
    } else {
      append_border("+", "-", "-", "+");
      set_theme(title_theme);
      append_title_row("|", "|", title, title_theme, true);
      append_border("+", "-", "+", "+");
    }
    themes_ = previous;
  } else {
    if (style_ == BorderStyle::Unicode) {
      append_border("╔", "═", "╦", "╗");
    } else {
      append_border("+", "-", "+", "+");
    }
  }
  if (!cols.empty()) {
    ThemeConfig cfg = theme_config();
    auto previous = themes_;
    Theme header_theme = previous.empty() ? Theme{} : previous[0];
    if (cfg.header_fg.has_value()) {
      header_theme.fg = cfg.header_fg;
    }
    if (cfg.header_bg.has_value()) {
      header_theme.bg = cfg.header_bg;
    }
    if (!cfg.header_style.empty()) {
      header_theme.style = cfg.header_style;
    }
    set_theme(header_theme);
    if (style_ == BorderStyle::Unicode) {
      append_row("║", "║", "║", cols, true);
      append_border("╠", "═", "╬", "╣");
    } else {
      append_row("|", "|", "|", cols, true);
      append_border("+", "-", "+", "+");
    }
    themes_ = previous;
  }
}

void Table::row(const std::vector<std::string>& cols) {
  if (style_ == BorderStyle::Unicode) {
    append_row("║", "║", "║", cols, false);
  } else {
    append_row("|", "|", "|", cols, false);
  }
}

void Table::boundary(bool light) {
  if (style_ == BorderStyle::Unicode) {
    append_border("╟", light ? "─" : "═", "╫", "╢");
  } else {
    append_border("+", light ? "-" : "=", "+", "+");
  }
}

void Table::close() {
  if (style_ == BorderStyle::Unicode) {
    append_border("╚", "═", "╩", "╝");
  } else {
    append_border("+", "-", "+", "+");
  }
}

void Table::banner(const std::string& text) {
  if (style_ == BorderStyle::Unicode) {
    append_border("╔", "═", "═", "╗");
    Theme banner_theme = theme_config().banner;
    append_title_row("║", "║", text, banner_theme, false);
    append_border("╚", "═", "═", "╝");
  } else {
    append_border("+", "-", "-", "+");
    Theme banner_theme = theme_config().banner;
    append_title_row("|", "|", text, banner_theme, false);
    append_border("+", "-", "-", "+");
  }
}

std::string Table::str() const {
  std::ostringstream out;
  for (const auto& line : lines_) {
    out << line << "\n";
  }
  return out.str();
}

void Table::append_border(const std::string& left, const std::string& fill,
                          const std::string& sep, const std::string& right) {
  std::string line;
  line += left;
  for (size_t i = 0; i < col_widths_.size(); ++i) {
    for (int j = 0; j < col_widths_[i] + 2; ++j) {
      line += fill;
    }
    line += (i + 1 == col_widths_.size() ? right : sep);
  }
  lines_.push_back(line);
}

void Table::append_row(const std::string& left, const std::string& sep,
                       const std::string& right,
                       const std::vector<std::string>& cols, bool centered) {
  std::string line;
  line += left;
  ThemeConfig cfg = theme_config();
  for (size_t i = 0; i < col_widths_.size(); ++i) {
    std::string cell = i < cols.size() ? cols[i] : "";
    std::string padded = pad_cell(cell, col_widths_[i], centered);
    if (use_color_) {
      Theme theme = i < themes_.size() ? themes_[i] : Theme{};
      if (centered && !cfg.header_style.empty()) {
        theme.style = cfg.header_style;
      }
      if (centered && cfg.header_fg.has_value()) {
        theme.fg = cfg.header_fg;
      }
      if (centered && cfg.header_bg.has_value()) {
        theme.bg = cfg.header_bg;
      }
      padded = apply_color(padded, theme);
    }
    line += padded;
    line += (i + 1 == col_widths_.size() ? right : sep);
  }
  lines_.push_back(line);
}

void Table::append_title_row(const std::string& left, const std::string& right,
                             const std::string& title, const Theme& theme,
                             bool centered) {
  int total = 0;
  for (int width : col_widths_) {
    total += width;
  }
  int inner = total + static_cast<int>(col_widths_.size()) * 2 +
              static_cast<int>(col_widths_.size()) - 1;
  std::string padded = pad_cell(title, inner - 2, centered);
  if (use_color_) {
    padded = apply_color(padded, theme);
  }
  std::string line;
  line += left;
  line += padded;
  line += right;
  lines_.push_back(line);
}

void Table::ensure_title_fits(const std::string& title) {
  int total = 0;
  for (int width : col_widths_) {
    total += width;
  }
  int inner = total + static_cast<int>(col_widths_.size()) * 2 +
              static_cast<int>(col_widths_.size()) - 1;
  int title_width = display_width(" " + title + " ");
  if (title_width <= inner) {
    return;
  }
  int extra = title_width - inner;
  if (!col_widths_.empty()) {
    col_widths_.back() += extra;
  }
}

std::string Table::pad_cell(const std::string& text, int width,
                            bool centered) const {
  std::string content = " " + text + " ";
  int pad = std::max(0, width + 2 - display_width(content));
  if (pad == 0) {
    return content;
  }
  if (centered) {
    int left = pad / 2;
    int right = pad - left;
    return std::string(left, ' ') + content + std::string(right, ' ');
  }
  return content + std::string(pad, ' ');
}

int Table::terminal_columns() {
#if defined(__unix__) || defined(__APPLE__)
  struct winsize w{};
  if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    return static_cast<int>(w.ws_col);
  }
#endif
  const char* cols = std::getenv("COLUMNS");
  if (cols) {
    int value = std::atoi(cols);
    if (value > 0) {
      return value;
    }
  }
  return 80;
}

void Table::adjust_widths() {
  int wild_count = 0;
  int fixed_sum = 0;
  for (int width : col_widths_) {
    if (width <= 0) {
      wild_count++;
    } else {
      fixed_sum += width;
    }
  }
  if (wild_count == 0) {
    return;
  }
  int cols = terminal_columns();
  int overhead = 1 + static_cast<int>(col_widths_.size()) *
                         3;  // borders + padding + separators
  int slack = cols - (fixed_sum + overhead);
  int wild = slack > 0 ? slack / wild_count : 1;
  int remainder = slack > 0 ? slack % wild_count : 0;
  for (int& width : col_widths_) {
    if (width <= 0) {
      width = wild + (remainder-- > 0 ? 1 : 0);
    }
  }
}

}  // namespace budget::ui
