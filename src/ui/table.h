#pragma once

#include <string>
#include <vector>

#include "color.h"

namespace budget::ui {

class Table {
 public:
  enum class BorderStyle { Ascii, Unicode };

  Table(std::vector<int> col_widths, bool use_color = true,
        BorderStyle style = BorderStyle::Unicode);

  void set_theme(const Theme& theme);
  void set_themes(const std::vector<Theme>& themes);

  void header(const std::vector<std::string>& cols,
              const std::string& title = "");
  void row(const std::vector<std::string>& cols);
  void boundary(bool light = false);
  void close();
  void banner(const std::string& text);

  std::string str() const;

 private:
  void append_border(const std::string& left, const std::string& fill,
                     const std::string& sep, const std::string& right);
  void append_row(const std::string& left, const std::string& sep,
                  const std::string& right,
                  const std::vector<std::string>& cols, bool centered);
  void append_title_row(const std::string& left, const std::string& right,
                        const std::string& title, const Theme& theme,
                        bool centered);
  void ensure_title_fits(const std::string& title);

  std::string pad_cell(const std::string& text, int width, bool centered) const;
  static int terminal_columns();
  void adjust_widths();

  std::vector<int> col_widths_;
  std::vector<Theme> themes_;
  bool use_color_ = true;
  BorderStyle style_ = BorderStyle::Unicode;
  std::vector<std::string> lines_;
};

}  // namespace budget::ui
