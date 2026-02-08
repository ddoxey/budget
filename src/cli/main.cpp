#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#if defined(HAVE_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#elif defined(HAVE_EDITLINE)
#include <editline/readline.h>
#endif

#include "../core/budget.h"
#include "../core/money.h"
#include "../core/repetition.h"
#include "../io/cache.h"
#include "../io/history_finder.h"
#include "../io/history_reader.h"
#include "../io/json_map.h"
#include "../io/paths.h"
#include "../ui/color_utils.h"
#include "../ui/dotchart.h"
#include "../ui/table.h"
#include "../ui/theme_config.h"
#include "completion.h"
#include "help.h"

namespace {

std::string ltrim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) {
    return "";
  }
  return s.substr(start);
}

std::string rtrim(const std::string& s) {
  size_t end = s.find_last_not_of(" \t\n\r");
  if (end == std::string::npos) {
    return "";
  }
  return s.substr(0, end + 1);
}

std::string trim(const std::string& s) { return rtrim(ltrim(s)); }

std::vector<std::string> split_tokens(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::vector<std::string> parse_quoted_regexes(const std::string& text) {
  std::vector<std::string> result;
  size_t i = 0;
  while (i < text.size() && result.size() < 2) {
    while (i < text.size() &&
           std::isspace(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i >= text.size()) {
      break;
    }
    if (text[i] == 'r' && i + 1 < text.size() && text[i + 1] == '\'') {
      i += 2;
    } else if (text[i] == '\'') {
      i += 1;
    } else {
      break;
    }
    std::string value;
    while (i < text.size()) {
      char c = text[i++];
      if (c == '\\' && i < text.size()) {
        char next = text[i++];
        value.push_back(c);
        value.push_back(next);
        continue;
      }
      if (c == '\'') {
        break;
      }
      value.push_back(c);
    }
    result.push_back(value);
  }
  return result;
}

std::optional<std::string> parse_quoted_string(const std::string& text,
                                               size_t& consumed) {
  consumed = 0;
  if (text.empty()) {
    return std::nullopt;
  }
  char quote = text[0];
  if (quote != '"' && quote != '\'') {
    return std::nullopt;
  }
  std::string value;
  size_t i = 1;
  while (i < text.size()) {
    char c = text[i++];
    if (c == '\\' && i < text.size()) {
      char next = text[i++];
      value.push_back(next);
      continue;
    }
    if (c == quote) {
      consumed = i;
      return value;
    }
    value.push_back(c);
  }
  return std::nullopt;
}

int terminal_columns() {
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

void print_mapping(const budget::io::StringMap& map) {
  if (map.empty()) {
    std::cout << "No header mappings configured." << std::endl;
    return;
  }
  std::vector<std::string> keys;
  keys.reserve(map.size());
  for (const auto& pair : map) {
    keys.push_back(pair.first);
  }
  std::sort(keys.begin(), keys.end());

  int col1 = 10;
  int col2 = 20;
  for (const auto& key : keys) {
    col1 = std::max(col1, static_cast<int>(key.size()));
    col2 = std::max(col2, static_cast<int>(map.at(key).size()));
  }

  budget::ui::Table table({col1, col2});
  table.header({"Internal", "Header"}, "Header Map");
  for (const auto& key : keys) {
    table.row({key, map.at(key)});
  }
  table.close();
  std::cout << table.str();
}

std::vector<budget::Profile> ensure_default_profiles(
    const std::vector<budget::Profile>& profiles) {
  if (!profiles.empty()) {
    return profiles;
  }
  return {{"Main", "Default Profile", 0.0}};
}

budget::Profile* find_profile(std::vector<budget::Profile>& profiles,
                              const std::string& name) {
  for (auto& profile : profiles) {
    if (profile.name == name) {
      return &profile;
    }
  }
  return nullptr;
}

bool is_valid_category(const std::string& cat) {
  if (cat.empty()) {
    return false;
  }
  for (char c : cat) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
          c == '&')) {
      return false;
    }
  }
  return true;
}

std::optional<int> parse_int(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }
  int value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  return value;
}

std::string join_tokens(const std::vector<std::string>& tokens,
                        size_t start, size_t end) {
  std::ostringstream out;
  for (size_t i = start; i < end; ++i) {
    if (i > start) {
      out << ' ';
    }
    out << tokens[i];
  }
  return out.str();
}

budget::ui::Theme to_theme(const budget::io::Cache::ThemeRecord& record) {
  budget::ui::Theme theme;
  if (record.fg >= 0) {
    theme.fg = record.fg;
  }
  if (record.bg >= 0) {
    theme.bg = record.bg;
  }
  theme.style = record.style;
  return theme;
}

budget::ui::Theme default_theme() {
  budget::ui::Theme theme;
  theme.fg = 254;
  theme.bg = 101;
  theme.style = "bold";
  return theme;
}

budget::ui::Theme resolve_default_theme(
    const std::unordered_map<std::string, budget::io::Cache::ThemeRecord>&
        themes) {
  auto it = themes.find("__default__");
  if (it != themes.end()) {
    return to_theme(it->second);
  }
  return default_theme();
}

void ensure_default_theme(budget::io::Cache& cache) {
  auto themes = cache.read_themes();
  if (themes.find("__default__") == themes.end()) {
    themes["__default__"] = {254, 101, "bold"};
    cache.write_themes(themes);
  }
}

budget::ui::Theme alternating_row_theme(size_t index) {
  auto cfg = budget::ui::theme_config();
  return (index % 2 == 0) ? cfg.row_even : cfg.row_odd;
}

std::optional<int> duration_to_days(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }
  char unit = text.back();
  if (unit != 'd' && unit != 'm' && unit != 'y') {
    return std::nullopt;
  }
  int value = 0;
  for (size_t i = 0; i + 1 < text.size(); ++i) {
    char c = text[i];
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  if (value <= 0) {
    return std::nullopt;
  }
  if (unit == 'd') {
    return value;
  }
  if (unit == 'm') {
    return value * 30;
  }
  return value * 365;
}

std::optional<std::string> duration_to_label(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }
  char unit = text.back();
  if (unit != 'd' && unit != 'm' && unit != 'y') {
    return std::nullopt;
  }
  int value = 0;
  for (size_t i = 0; i + 1 < text.size(); ++i) {
    char c = text[i];
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  if (value <= 0) {
    return std::nullopt;
  }
  std::string label;
  if (unit == 'd') {
    label = "day";
  } else if (unit == 'm') {
    label = "month";
  } else {
    label = "year";
  }
  if (value != 1) {
    label += "s";
  }
  return std::to_string(value) + " " + label;
}

int days_away(const budget::Date& target) {
  budget::Date today = budget::today_est();
  return target.days_since_epoch() - today.days_since_epoch();
}

#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
static std::vector<std::string> g_matches;
static size_t g_match_index = 0;

char* completion_generator(const char* text, int state) {
  if (state == 0) {
    g_match_index = 0;
    std::string buffer = rl_line_buffer ? rl_line_buffer : "";
    size_t cursor = static_cast<size_t>(rl_point);
    g_matches = budget::cli::completion_candidates(buffer, cursor);
  }
  if (g_match_index >= g_matches.size()) {
    return nullptr;
  }
  const std::string& match = g_matches[g_match_index++];
  return strdup(match.c_str());
}

char** completer(const char* text, int start, int end) {
  (void)text;
  (void)start;
  (void)end;
  rl_attempted_completion_over = 1;
  return rl_completion_matches(text, completion_generator);
}
#endif

}  // namespace

int main() {
  std::string prompt = "Budget: ";
  std::string line;

  const std::string map_path = budget::io::header_map_path();
  std::string active_profile = "Main";
  budget::io::Cache cache("budget", active_profile);
  auto profiles = ensure_default_profiles(
      cache.read_profiles({{"Main", "Default Profile", 0.0}}));
  auto session_profile = cache.read_session_profile();
  if (session_profile.has_value() &&
      find_profile(profiles, *session_profile)) {
    active_profile = *session_profile;
    cache = budget::io::Cache("budget", active_profile);
  }
  ensure_default_theme(cache);
  budget::ui::refresh_theme_config();

#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
  budget::cli::set_category_provider([&cache]() {
    std::vector<std::string> cats;
    auto types = cache.read_transaction_types();
    cats.reserve(types.size());
    for (const auto& type : types) {
      cats.push_back(type.category);
    }
    return cats;
  });
  budget::cli::set_profile_provider([&cache]() {
    std::vector<std::string> names;
    auto profiles = ensure_default_profiles(cache.read_profiles());
    names.reserve(profiles.size());
    for (const auto& profile : profiles) {
      names.push_back(profile.name);
    }
    return names;
  });
  rl_attempted_completion_function = completer;
#endif

  while (true) {
#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
    char* input = readline(prompt.c_str());
    if (!input) {
      break;
    }
    line = input;
    if (!line.empty()) {
      add_history(input);
    }
    free(input);
#else
    std::cout << prompt;
    if (!std::getline(std::cin, line)) {
      break;
    }
#endif
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line == "exit" || line == "quit" || line == "q" || line == "x") {
      break;
    }

    budget::ui::refresh_theme_config();

    if (line == "help") {
      budget::ui::Table table({20, -1});
      table.header({"Command", "Description"}, "Help");
      std::vector<std::string> keys;
      for (const auto& pair : budget::cli::help_topics()) {
        keys.push_back(pair.first);
      }
      std::sort(keys.begin(), keys.end());
      for (const auto& key : keys) {
        table.row({key, budget::cli::help_topics().at(key).summary});
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line == "clear") {
      std::cout << "\x1b[2J\x1b[H" << std::flush;
      continue;
    }

    if (line == "themeconfig") {
      auto cfg = budget::ui::theme_config();
      budget::ui::Table table({18, -1});
      table.header({"Key", "Value"}, "Theme Config");
      size_t row_i = 0;
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"title_fg", cfg.title.fg.has_value()
                                 ? std::to_string(cfg.title.fg.value())
                                 : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"title_bg", cfg.title.bg.has_value()
                                 ? std::to_string(cfg.title.bg.value())
                                 : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"title_style", cfg.title.style});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"header_fg", cfg.header_fg.has_value()
                                  ? std::to_string(cfg.header_fg.value())
                                  : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"header_bg", cfg.header_bg.has_value()
                                  ? std::to_string(cfg.header_bg.value())
                                  : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"header_style", cfg.header_style});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"banner_fg", cfg.banner.fg.has_value()
                                  ? std::to_string(cfg.banner.fg.value())
                                  : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"banner_bg", cfg.banner.bg.has_value()
                                  ? std::to_string(cfg.banner.bg.value())
                                  : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"banner_style", cfg.banner.style});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_even_fg", cfg.row_even.fg.has_value()
                                    ? std::to_string(cfg.row_even.fg.value())
                                    : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_even_bg", cfg.row_even.bg.has_value()
                                    ? std::to_string(cfg.row_even.bg.value())
                                    : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_even_style", cfg.row_even.style});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_odd_fg", cfg.row_odd.fg.has_value()
                                   ? std::to_string(cfg.row_odd.fg.value())
                                   : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_odd_bg", cfg.row_odd.bg.has_value()
                                   ? std::to_string(cfg.row_odd.bg.value())
                                   : "None"});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"row_odd_style", cfg.row_odd.style});
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line.rfind("help ", 0) == 0) {
      std::string topic = trim(line.substr(std::string("help ").size()));
      auto it = budget::cli::help_topics().find(topic);
      if (it == budget::cli::help_topics().end()) {
        std::cerr << "Unknown help topic: " << topic << std::endl;
        continue;
      }
      std::cout << it->second.details << std::endl;
      continue;
    }

    if (line == "headers") {
      auto map = budget::io::read_json_map(map_path);
      if (!map.has_value()) {
        std::cout << "No header map found at: " << map_path << std::endl;
        continue;
      }
      print_mapping(*map);
      continue;
    }

    if (line == "profile" || line.rfind("profile ", 0) == 0) {
      auto profiles = ensure_default_profiles(cache.read_profiles());
      std::string name = trim(line.substr(std::string("profile").size()));
      if (name.empty()) {
        std::cout << "Profile: " << active_profile << std::endl;
        continue;
      }
      if (!find_profile(profiles, name)) {
        std::cerr << "Invalid profile name: " << name << std::endl;
        continue;
      }
      active_profile = name;
      cache = budget::io::Cache("budget", active_profile);
      cache.write_session_profile(active_profile);
      std::cout << "Profile: " << active_profile << std::endl;
      continue;
    }

    if (line.rfind("balance", 0) == 0) {
      std::string rest = trim(line.substr(std::string("balance").size()));
      auto profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      auto* profile = find_profile(profiles, active_profile);
      if (!profile) {
        std::cerr << "Profile not found: " << active_profile << std::endl;
        continue;
      }
      if (!rest.empty()) {
        std::string raw = rest;
        std::string op;
        if (raw.rfind("+=", 0) == 0 || raw.rfind("-=", 0) == 0) {
          op = raw.substr(0, 2);
          raw = trim(raw.substr(2));
        }
        if (!budget::Money::matches(raw)) {
          std::cerr << "Invalid monetary value: " << raw << std::endl;
          continue;
        }
        auto amount = budget::Money::parse(raw);
        if (!amount.has_value()) {
          std::cerr << "Invalid amount: " << raw << std::endl;
          continue;
        }
        double new_balance = *amount;
        if (!op.empty()) {
          if (op == "+=") {
            new_balance = profile->balance + *amount;
          } else if (op == "-=") {
            new_balance = profile->balance - *amount;
          }
        }
        profile->balance = new_balance;
        cache.write_profiles(profiles);
      }
      budget::Money money(profile->balance, "$");
      std::cout << "Balance: " << money.str() << std::endl;
      continue;
    }

    if (line == "profiles") {
      auto profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      budget::ui::Table table({12, -1, 12});
      table.header({"Name", "Description", "Balance"}, "Profiles");
      size_t row_i = 0;
      for (const auto& profile : profiles) {
        std::string name = profile.name;
        if (profile.name == active_profile) {
          name += "*";
        }
        table.set_theme(alternating_row_theme(row_i++));
        budget::Money bal(profile.balance, "$");
        table.row({name, profile.description, bal.str()});
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line == "cats") {
      auto types = cache.read_transaction_types();
      if (types.empty()) {
        std::cout << "No categories configured." << std::endl;
        continue;
      }
      std::vector<std::string> cats;
      cats.reserve(types.size());
      for (const auto& type : types) {
        cats.push_back(type.category);
      }
      std::sort(cats.begin(), cats.end());

      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);

      constexpr int kColumns = 3;
      size_t rows = (cats.size() + kColumns - 1) / kColumns;
      budget::ui::Table table({15, 15, 15});
      table.header({}, active_profile + " Categories");
      for (size_t row = 0; row < rows; ++row) {
        std::vector<std::string> cells;
        std::vector<budget::ui::Theme> themes_row;
        cells.reserve(kColumns);
        themes_row.reserve(kColumns);
        for (int col = 0; col < kColumns; ++col) {
          size_t idx = row + static_cast<size_t>(col) * rows;
          if (idx < cats.size()) {
            const auto& cat = cats[idx];
            cells.push_back(cat);
            auto it_theme = themes.find(cat);
            if (it_theme != themes.end()) {
              themes_row.push_back(to_theme(it_theme->second));
            } else {
              themes_row.push_back(fallback);
            }
          } else {
            cells.emplace_back();
            themes_row.push_back(fallback);
          }
        }
        table.set_themes(themes_row);
        table.row(cells);
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line == "exceptions") {
      auto exceptions = cache.read_exceptions();
      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);
      if (exceptions.empty()) {
        std::cout << "No exceptions configured." << std::endl;
        continue;
      }
      budget::ui::Table table({10, -1, 12});
      table.header({"Date", "Category", "Amount"}, "Exceptions");
      for (const auto& exc : exceptions) {
        auto it_theme = themes.find(exc.category);
        if (it_theme != themes.end()) {
          table.set_theme(to_theme(it_theme->second));
        } else {
          table.set_theme(fallback);
        }
        std::ostringstream amt;
        amt.setf(std::ios::fixed);
        amt.precision(2);
        amt << exc.amount;
        table.row({exc.date.to_mm_dd_yyyy(), exc.category, amt.str()});
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line == "themes") {
      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);
      if (themes.empty()) {
        std::cout << "No themes configured." << std::endl;
        continue;
      }
      budget::ui::Table table({16, 8, 8, -1});
      table.header({"Category", "FG", "BG", "Style"}, "Themes");
      std::vector<std::string> cats;
      cats.reserve(themes.size());
      for (const auto& pair : themes) {
        cats.push_back(pair.first);
      }
      table.set_theme(fallback);
      if (themes.find("__default__") != themes.end()) {
        const auto& t = themes["__default__"];
        table.row(
            {"(default)", std::to_string(t.fg), std::to_string(t.bg), t.style});
      } else {
        table.row({"(default)", "254", "101", "bold"});
      }
      cats.erase(std::remove(cats.begin(), cats.end(), "__default__"),
                 cats.end());
      std::sort(cats.begin(), cats.end());
      for (const auto& cat : cats) {
        const auto& t = themes[cat];
        table.set_theme(to_theme(t));
        table.row({cat, std::to_string(t.fg), std::to_string(t.bg), t.style});
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line.rfind("themes ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("themes ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.empty()) {
        std::cerr
            << "Usage: themes [randomize [category]|rotate|reset|show default]"
            << std::endl;
        continue;
      }
      std::string action = tokens[0];
      std::string subaction = tokens.size() > 1 ? tokens[1] : "";
      auto types = cache.read_transaction_types();
      std::vector<std::string> categories;
      categories.reserve(types.size());
      for (const auto& type : types) {
        categories.push_back(type.category);
      }
      auto themes = cache.read_themes();
      if (action == "show" && subaction == "default") {
        auto fallback = resolve_default_theme(themes);
        budget::ui::Table table({16, 8, 8, -1});
        table.header({"Category", "FG", "BG", "Style"}, "Themes");
        table.set_theme(fallback);
        table.row({"(default)", std::to_string(fallback.fg.value_or(105)),
                   std::to_string(fallback.bg.value_or(121)),
                   fallback.style.empty() ? "bold" : fallback.style});
        table.close();
        std::cout << table.str();
        continue;
      } else if (action == "randomize") {
        std::string cat = tokens.size() > 1 ? tokens[1] : "";
        auto rand_color_pair = []() {
          int r = std::rand() % 6;
          int g = std::rand() % 6;
          int b = std::rand() % 6;
          auto base = budget::ui::Color::FromAnsiCube(r, g, b);
          auto bg = base.GetComplementaryColorCube();
          auto fg = bg.GetContrastingColorCube(100);
          auto bg_cube = bg.ToAnsiCube();
          auto fg_cube = fg.ToAnsiCube();
          int bg_code = 16 + (bg_cube[0] * 36) + (bg_cube[1] * 6) + bg_cube[2];
          int fg_code = 16 + (fg_cube[0] * 36) + (fg_cube[1] * 6) + fg_cube[2];
          if (bg_code == fg_code) {
            fg_code = (fg_code + 36) % 216 + 16;
          }
          return std::pair<int, int>{fg_code, bg_code};
        };
        if (!cat.empty()) {
          auto pair = rand_color_pair();
          themes[cat] = {pair.first, pair.second, "bold"};
        } else {
          for (const auto& c : categories) {
            auto pair = rand_color_pair();
            themes[c] = {pair.first, pair.second, "bold"};
          }
        }
        cache.write_themes(themes);
      } else if (action == "rotate") {
        if (categories.empty()) {
          std::cerr << "No categories available to rotate." << std::endl;
          continue;
        }
        std::vector<budget::io::Cache::ThemeRecord> seq;
        seq.reserve(categories.size());
        for (const auto& cat : categories) {
          auto it = themes.find(cat);
          if (it != themes.end()) {
            seq.push_back(it->second);
          } else {
            seq.push_back({105, 121, "bold"});
          }
        }
        if (!seq.empty()) {
          std::rotate(seq.rbegin(), seq.rbegin() + 1, seq.rend());
          for (size_t i = 0; i < categories.size(); ++i) {
            themes[categories[i]] = seq[i];
          }
          cache.write_themes(themes);
        }
      } else if (action == "reset") {
        themes.clear();
        themes["__default__"] = {254, 101, "bold"};
        cache.write_themes(themes);
      } else {
        std::cerr << "Unrecognized themes action: " << action << std::endl;
        continue;
      }
      std::cout << "Themes updated." << std::endl;
      continue;
    }

    if (line == "trans" || line == "transactions") {
      auto types = cache.read_transaction_types();
      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);
      if (types.empty()) {
        std::cout << "No transaction types configured." << std::endl;
        continue;
      }
      double monthly_balance = 0.0;
      bool abbreviated = (line == "trans");
      budget::ui::Table table(abbreviated
                                  ? std::vector<int>{12, -1, 12}
                                  : std::vector<int>{12, -1, 12, -1, 12});
      std::string title = active_profile + " Transaction Types";
      if (abbreviated) {
        table.header({"Category", "Repeats", "Amount"}, title);
      } else {
        table.header({"Category", "Repeats", "Amount", "Match Description",
                      "Match Amount"},
                     title);
      }
      for (const auto& type : types) {
        auto it_theme = themes.find(type.category);
        if (it_theme != themes.end()) {
          table.set_theme(to_theme(it_theme->second));
        } else {
          table.set_theme(fallback);
        }
        std::ostringstream amt;
        amt.setf(std::ios::fixed);
        amt.precision(2);
        amt << type.amount;
        budget::Repetition rep(type.repetition);
        monthly_balance += rep.monthly_factor() * type.amount;
        if (abbreviated) {
          table.row({type.category, rep.to_string(), amt.str()});
        } else {
          table.row({type.category, rep.to_string(), amt.str(),
                     type.description_regex, type.debit_regex});
        }
      }
      table.close();
      std::cout << table.str();
      std::ostringstream balance_text;
      balance_text.setf(std::ios::fixed);
      balance_text.precision(2);
      balance_text << active_profile << " Monthly Balance: " << monthly_balance;
      budget::ui::Table banner({-1});
      banner.banner(balance_text.str());
      std::cout << banner.str();
      continue;
    }

    if (line == "status") {
      auto profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      auto exceptions = cache.read_exceptions();
      auto types = cache.read_transaction_types();
      auto themes = cache.read_themes();
      auto csv = budget::io::latest_csv_path();
      std::string cfg_dir = budget::io::config_dir();
      std::string dl_dir = budget::io::download_dir();
      std::string cache_dir = cache.cache_dir();

      std::string profile_desc;
      double balance = 0.0;
      for (const auto& profile : profiles) {
        if (profile.name == active_profile) {
          profile_desc = profile.description;
          balance = profile.balance;
          break;
        }
      }
      budget::Money bal(balance, "$");

      budget::ui::Table table({18, -1});
      table.header({"Key", "Value"}, "Status");
      size_t row_i = 0;
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Profile", profile_desc});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Balance", bal.str()});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Categories", std::to_string(types.size())});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Exceptions", std::to_string(exceptions.size())});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Themes", std::to_string(themes.size())});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Cache Dir", cache_dir.empty() ? "None" : cache_dir});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Config Dir", cfg_dir.empty() ? "None" : cfg_dir});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Download Dir", dl_dir.empty() ? "None" : dl_dir});
      std::string csv_name =
          csv.has_value() ? std::filesystem::path(*csv).filename().string()
                          : "None";
      std::string map_name =
          map_path.empty()
              ? "None"
              : std::filesystem::path(map_path).filename().string();
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"CSV File", csv_name});
      table.set_theme(alternating_row_theme(row_i++));
      table.row({"Header Map File", map_name});
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line.rfind("run ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("run ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.empty()) {
        std::cerr << "Usage: run <number>(d|m|y) [<start-date>]" << std::endl;
        continue;
      }
      auto days = duration_to_days(tokens[0]);
      if (!days.has_value()) {
        std::cerr << "Usage: run <number>(d|m|y) [<start-date>]" << std::endl;
        continue;
      }
      int days_offset = 0;
      if (tokens.size() >= 2) {
        auto date = budget::Date::parse_mm_dd_yyyy(tokens[1]);
        if (!date.has_value()) {
          std::cerr << "Invalid date: " << tokens[1] << std::endl;
          continue;
        }
        days_offset = days_away(*date);
        if (days_offset < 0) {
          std::cerr << "Invalid date: " << tokens[1] << std::endl;
          continue;
        }
      }

      auto csv_path = budget::io::latest_csv_path();
      if (!csv_path.has_value()) {
        std::cerr
            << "No CSV found. Set BUDGET_CSV or place a .csv in ~/Downloads."
            << std::endl;
        continue;
      }

      std::vector<budget::Profile> profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      double balance = 0.0;
      for (const auto& profile : profiles) {
        if (profile.name == active_profile) {
          balance = profile.balance;
          break;
        }
      }

      auto transaction_types = cache.read_transaction_types();
      auto exceptions = cache.read_exceptions();
      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);

      try {
        auto history =
            budget::io::read_transaction_history(*csv_path, map_path);
        auto filtered =
            budget::filter_exceptions(exceptions, budget::today_est());
        budget::Budget budget_model(balance, transaction_types, filtered,
                                    history.transactions, *days, days_offset);

        budget::ui::Table table({10, -1, 12, 12});
        budget::Money opening(balance, "$");
        table.header({"date", "category", "amount", "balance"},
                     active_profile + " Opening Balance: " + opening.str());
        std::vector<double> balances;
        std::string last_month;
        for (const auto& event : budget_model.events()) {
          std::string month = event.get_date().to_mm_dd_yyyy().substr(0, 2);
          if (!last_month.empty() && month != last_month) {
            table.boundary(true);
          }
          last_month = month;
          auto it_theme = themes.find(event.get_category());
          if (it_theme != themes.end()) {
            table.set_theme(to_theme(it_theme->second));
          } else {
            table.set_theme(fallback);
          }
          std::ostringstream amt;
          amt.setf(std::ios::fixed);
          amt.precision(2);
          amt << event.get_amount();
          std::ostringstream bal;
          bal.setf(std::ios::fixed);
          bal.precision(2);
          bal << event.get_balance();
          table.row({event.get_date().to_mm_dd_yyyy(), event.get_category(),
                     amt.str(), bal.str()});
          balances.push_back(event.get_balance());
        }
        table.close();
        std::cout << table.str();

        int remaining_days = *days;
        int years = remaining_days / 365;
        remaining_days -= years * 365;
        int months = remaining_days / 30;
        remaining_days -= months * 30;
        std::ostringstream elapsed;
        bool first = true;
        if (years > 0) {
          elapsed << years << " years";
          first = false;
        }
        if (months > 0) {
          if (!first) {
            elapsed << ", ";
          }
          elapsed << months << " months";
          first = false;
        }
        if (remaining_days > 0 || (years == 0 && months == 0)) {
          if (!first) {
            elapsed << ", ";
          }
          elapsed << remaining_days << " days";
        }
        std::cout << elapsed.str() << std::endl;

        cache.write_lasts(budget_model.last_occurrences());

        std::optional<std::string> crash_text;
        if (*days > 60) {
          auto chokepoints = budget_model.chokepoints();
          if (chokepoints.minimum().has_value()) {
            auto min = chokepoints.minimum().value();
            std::string title =
                "Eye of the needle: " + min.date().to_mm_dd_yyyy() + " : ";
            std::ostringstream min_bal;
            min_bal.setf(std::ios::fixed);
            min_bal.precision(2);
            min_bal << min.balance();
            title += min_bal.str();
            budget::ui::Table eye_table({10, 12});
            eye_table.header({"date", "balance"}, title);
            size_t row_i = 0;
            for (const auto& cp : chokepoints.chokepoints()) {
              eye_table.set_theme(alternating_row_theme(row_i++));
              std::ostringstream bal;
              bal.setf(std::ios::fixed);
              bal.precision(2);
              bal << cp.balance();
              eye_table.row({cp.date().to_mm_dd_yyyy(), bal.str()});
            }
            eye_table.close();
            std::cout << eye_table.str();
            auto crash = chokepoints.crash_date();
            if (crash.has_value() && min.balance() >= 0) {
              crash_text = "Predicted Crash Date: " + crash->to_mm_dd_yyyy();
            }
          }
        }

        std::cout << std::endl;
        int width = terminal_columns() - 1;
        if (width < 20) {
          width = 20;
        }
        auto chart = budget::ui::render_dotchart(balances, width);
        if (chart.has_value()) {
          std::cout << *chart << std::endl;
        }
        if (crash_text.has_value()) {
          budget::ui::Table banner({-1});
          banner.banner(*crash_text);
          std::cout << banner.str();
        }
      } catch (const std::exception& ex) {
        std::cerr << "run error: " << ex.what() << std::endl;
      }
      continue;
    }

    if (line == "totals" || line.rfind("totals ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("totals").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() != 1) {
        std::cerr << "Usage: totals <number>(d|m|y)" << std::endl;
        continue;
      }
      auto days = duration_to_days(tokens[0]);
      if (!days.has_value()) {
        std::cerr << "Usage: totals <number>(d|m|y)" << std::endl;
        continue;
      }
      auto duration_label = duration_to_label(tokens[0]);
      if (!duration_label.has_value()) {
        std::cerr << "Usage: totals <number>(d|m|y)" << std::endl;
        continue;
      }

      std::vector<budget::Profile> profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      double balance = 0.0;
      for (const auto& profile : profiles) {
        if (profile.name == active_profile) {
          balance = profile.balance;
          break;
        }
      }

      auto transaction_types = cache.read_transaction_types();
      auto exceptions = cache.read_exceptions();
      auto themes = cache.read_themes();
      auto fallback = resolve_default_theme(themes);
      auto filtered = budget::filter_exceptions(exceptions, budget::today_est());

      std::vector<budget::Transaction> history;
      auto csv_path = budget::io::latest_csv_path();
      if (csv_path.has_value()) {
        try {
          auto history_data =
              budget::io::read_transaction_history(*csv_path, map_path);
          history = history_data.transactions;
        } catch (const std::exception& ex) {
          std::cerr << "totals error: " << ex.what() << std::endl;
        }
      }

      auto lasts = cache.read_lasts();
      if (history.empty() && !lasts.empty()) {
        for (const auto& pair : lasts) {
          budget::Transaction transaction;
          std::string date = pair.second;
          std::replace(date.begin(), date.end(), '-', '/');
          transaction.fields["transaction_date"] = date;
          transaction.fields["cat"] = pair.first;
          history.push_back(std::move(transaction));
        }
      }

      budget::Budget budget_model(balance, transaction_types, filtered, history,
                                  *days);
      auto totals = budget_model.totals();
      if (totals.empty()) {
        std::cout << "No totals available." << std::endl;
        continue;
      }

      std::vector<std::pair<std::string, double>> rows;
      rows.reserve(totals.size());
      for (const auto& pair : totals) {
        rows.push_back(pair);
      }
      std::sort(rows.begin(), rows.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });

      budget::ui::Table table({-1, 15});
      table.header({}, active_profile + " Totals for " + *duration_label);
      double net_total = 0.0;
      for (const auto& row : rows) {
        auto it_theme = themes.find(row.first);
        if (it_theme != themes.end()) {
          table.set_theme(to_theme(it_theme->second));
        } else {
          table.set_theme(fallback);
        }
        net_total += row.second;
        budget::Money money(row.second, "$");
        table.row({row.first, money.str()});
      }
      table.close();
      std::cout << table.str();
      budget::Money net_money(net_total, "$");
      std::cout << active_profile << " Net Total for " << *duration_label
                << ": "
                << net_money.str() << std::endl;
      continue;
    }

    if (line.rfind("save", 0) == 0) {
      std::cout
          << "Save is implicit for now; update commands persist immediately."
          << std::endl;
      continue;
    }

    if (line == "reload") {
      std::cout << "Reloading transaction types..." << std::endl;
      auto types = cache.read_transaction_types();
      std::cout << "Reloading exceptions..." << std::endl;
      auto exceptions = cache.read_exceptions();
      auto filtered = budget::filter_exceptions(exceptions, budget::today_est());
      cache.write_exceptions(filtered);

      std::vector<budget::Transaction> history;
      auto csv_path = budget::io::latest_csv_path();
      if (csv_path.has_value()) {
        std::cout << "Reading CSV: " << *csv_path << std::endl;
        try {
          auto history_data =
              budget::io::read_transaction_history(*csv_path, map_path);
          history = history_data.transactions;
        } catch (const std::exception& ex) {
          std::cerr << "reload error: " << ex.what() << std::endl;
        }
      } else {
        std::cout << "No CSV found." << std::endl;
      }

      auto lasts = cache.read_lasts();
      if (history.empty() && !lasts.empty()) {
        std::cout << "Using cached last occurrences." << std::endl;
        for (const auto& pair : lasts) {
          budget::Transaction transaction;
          std::string date = pair.second;
          std::replace(date.begin(), date.end(), '-', '/');
          transaction.fields["transaction_date"] = date;
          transaction.fields["cat"] = pair.first;
          history.push_back(std::move(transaction));
        }
      }

      if (!history.empty()) {
        std::cout << "Recomputing last occurrences..." << std::endl;
        budget::Budget budget_model(0.0, types, filtered, history, 36, 0);
        cache.write_lasts(budget_model.last_occurrences());
      }
      std::cout << "Reloaded." << std::endl;
      continue;
    }

    if (line.rfind("copy ", 0) == 0) {
      std::string args = trim(line.substr(std::string("copy ").size()));
      if (args.empty()) {
        std::cerr << "Invalid copy command: " << line << std::endl;
        continue;
      }
      auto tokens = split_tokens(args);
      if (tokens.empty()) {
        std::cerr << "Invalid copy command: " << line << std::endl;
        continue;
      }
      std::string copy_type = tokens[0];
      std::string remainder =
          trim(args.substr(std::min(args.size(), copy_type.size())));
      if (copy_type == "profile") {
        auto copy_tokens = split_tokens(remainder);
        if (copy_tokens.size() != 2) {
          std::cerr << "Usage: copy profile <from> <to>" << std::endl;
          continue;
        }
        std::string from_name = copy_tokens[0];
        std::string to_name = copy_tokens[1];
        if (from_name == to_name) {
          std::cerr << "Cannot copy " << from_name << " to itself"
                    << std::endl;
          continue;
        }
        auto profiles = ensure_default_profiles(cache.read_profiles());
        auto* from_profile = find_profile(profiles, from_name);
        if (!from_profile) {
          std::cerr << "Unrecognized profile: " << from_name << std::endl;
          continue;
        }
        cache.copy_profile(from_name, to_name);
        profiles.push_back(
            {to_name, "Copy of " + from_profile->description, 0.0});
        cache.write_profiles(profiles);
#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
        budget::cli::set_profile_provider([&cache]() {
          std::vector<std::string> names;
          auto profiles = ensure_default_profiles(cache.read_profiles());
          names.reserve(profiles.size());
          for (const auto& profile : profiles) {
            names.push_back(profile.name);
          }
          return names;
        });
#endif
        continue;
      }
      std::cerr << "Unsupported copy type: " << copy_type << std::endl;
      continue;
    }

    if (line == "lasts") {
      auto types = cache.read_transaction_types();
      if (types.empty()) {
        std::cerr << "No transaction types configured." << std::endl;
        continue;
      }
      auto lasts = cache.read_lasts();
      if (lasts.empty()) {
        auto csv_path = budget::io::latest_csv_path();
        if (!csv_path.has_value()) {
          std::cerr
              << "No CSV found. Set BUDGET_CSV or place a .csv in ~/Downloads."
              << std::endl;
          continue;
        }
        try {
          auto history =
              budget::io::read_transaction_history(*csv_path, map_path);
          auto exceptions = cache.read_exceptions();
          auto filtered =
              budget::filter_exceptions(exceptions, budget::today_est());
          budget::Budget budget_model(0.0, types, filtered,
                                      history.transactions, 36, 0);
          lasts = budget_model.last_occurrences();
          cache.write_lasts(lasts);
        } catch (const std::exception& ex) {
          std::cerr << "lasts error: " << ex.what() << std::endl;
          continue;
        }
      }
      if (lasts.empty()) {
        std::cout << "No last occurrences available." << std::endl;
        continue;
      }
      budget::ui::Table table({16, 12});
      table.header({"Category", "Date"}, "Last Occurrences");
      std::vector<std::string> cats;
      cats.reserve(lasts.size());
      for (const auto& pair : lasts) {
        cats.push_back(pair.first);
      }
      std::sort(cats.begin(), cats.end());
      for (const auto& cat : cats) {
        table.row({cat, lasts[cat]});
      }
      table.close();
      std::cout << table.str();
      continue;
    }

    if (line.rfind("update header ", 0) == 0) {
      std::string rest = line.substr(std::string("update header ").size());
      auto tokens = split_tokens(rest);
      if (tokens.size() < 2) {
        std::cerr << "Usage: update header <internal> <header>" << std::endl;
        continue;
      }
      std::string internal = tokens[0];
      std::string header = trim(rest.substr(internal.size()));
      if (!header.empty() && header[0] == ' ') {
        header = trim(header);
      }
      auto map =
          budget::io::read_json_map(map_path).value_or(budget::io::StringMap{});
      map[internal] = header;
      if (!map_path.empty()) {
        std::filesystem::create_directories(
            std::filesystem::path(map_path).parent_path());
      }
      if (!budget::io::write_json_map(map_path, map)) {
        std::cerr << "Failed to write header map: " << map_path << std::endl;
        continue;
      }
      std::cout << "Updated header mapping: " << internal << " -> " << header
                << std::endl;
      continue;
    }

    if (line.rfind("update profile ", 0) == 0) {
      std::string rest =
          trim(line.substr(std::string("update profile ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 2) {
        std::cerr
            << "Usage: update profile <name> <description> [<balance>]"
            << std::endl;
        continue;
      }
      std::string name = tokens[0];
      std::string remainder = trim(rest.substr(name.size()));
      std::string desc;
      std::optional<double> balance_override;
      if (!remainder.empty() &&
          (remainder[0] == '"' || remainder[0] == '\'')) {
        size_t consumed = 0;
        auto parsed = parse_quoted_string(remainder, consumed);
        if (!parsed.has_value()) {
          std::cerr << "Unable to parse description string." << std::endl;
          continue;
        }
        desc = *parsed;
        std::string tail = trim(remainder.substr(consumed));
        if (!tail.empty()) {
          auto amount = budget::Money::parse(tail);
          if (!amount.has_value()) {
            std::cerr << "Invalid balance amount: " << tail << std::endl;
            continue;
          }
          balance_override = *amount;
        }
      } else {
        auto rem_tokens = split_tokens(remainder);
        if (rem_tokens.size() >= 2) {
          auto amount = budget::Money::parse(rem_tokens.back());
          if (amount.has_value()) {
            balance_override = *amount;
            desc = join_tokens(rem_tokens, 0, rem_tokens.size() - 1);
          } else {
            desc = remainder;
          }
        } else {
          desc = remainder;
        }
      }
      auto profiles = ensure_default_profiles(cache.read_profiles());
      auto* profile = find_profile(profiles, name);
      if (profile) {
        profile->description = desc;
        if (balance_override.has_value()) {
          profile->balance = *balance_override;
        }
        std::cout << "updated profile " << name << ": " << desc << std::endl;
      } else {
        profiles.push_back(
            {name, desc, balance_override.value_or(0.0)});
        std::cout << "Added profile " << name << ": " << desc << std::endl;
      }
      cache.write_profiles(profiles);
#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
      budget::cli::set_profile_provider([&cache]() {
        std::vector<std::string> names;
        auto profiles = ensure_default_profiles(cache.read_profiles());
        names.reserve(profiles.size());
        for (const auto& profile : profiles) {
          names.push_back(profile.name);
        }
        return names;
      });
#endif
      continue;
    }

    if (line.rfind("update exception ", 0) == 0) {
      std::string rest =
          trim(line.substr(std::string("update exception ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 3) {
        std::cerr << "Usage: update exception <cat> <mm-dd-yyyy> <amount>"
                  << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      if (!is_valid_category(cat)) {
        std::cerr << "Bad category name: " << cat << std::endl;
        continue;
      }
      auto date = budget::Date::parse_mm_dd_yyyy(tokens[1]);
      if (!date.has_value()) {
        std::cerr << "Invalid date: " << tokens[1] << std::endl;
        continue;
      }
      auto amount = budget::Money::parse(tokens[2]);
      if (!amount.has_value()) {
        std::cerr << "Invalid monetary value: " << tokens[2] << std::endl;
        continue;
      }
      auto exceptions = cache.read_exceptions();
      bool updated = false;
      for (auto& exc : exceptions) {
        if (exc.category == cat && exc.date == *date) {
          exc.amount = *amount;
          updated = true;
          std::cout << "Update exception " << cat << ": " << *amount << " on "
                    << date->to_mm_dd_yyyy() << std::endl;
          break;
        }
      }
      if (!updated) {
        exceptions.push_back({cat, *date, *amount});
        std::cout << "Add exception " << cat << ": " << *amount << " on "
                  << date->to_mm_dd_yyyy() << std::endl;
      }
      cache.write_exceptions(exceptions);
      continue;
    }

    if (line.rfind("update transaction ", 0) == 0) {
      std::string rest =
          trim(line.substr(std::string("update transaction ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 3) {
        std::cerr << "Usage: update transaction <cat> <when>[/<repeat>] "
                     "<amount> [<desc>] [<amount>]"
                  << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      if (!is_valid_category(cat)) {
        std::cerr << "Bad category name: " << cat << std::endl;
        continue;
      }
      std::string repetition = tokens[1];
      if (!budget::Repetition::parse(repetition).has_value()) {
        std::cerr << "Invalid repetition pattern: " << repetition << std::endl;
        continue;
      }
      auto amount = budget::Money::parse(tokens[2]);
      if (!amount.has_value()) {
        std::cerr << "Invalid monetary value: " << tokens[2] << std::endl;
        continue;
      }
      std::string desc_regex;
      std::string debit_regex;
      bool desc_set = false;
      bool debit_set = false;
      std::string remainder = trim(rest.substr(
          tokens[0].size() + tokens[1].size() + tokens[2].size() + 2));
      auto regexes = parse_quoted_regexes(remainder);
      if (regexes.size() >= 1) {
        desc_regex = regexes[0];
        desc_set = true;
      }
      if (regexes.size() >= 2) {
        debit_regex = regexes[1];
        debit_set = true;
      }

      auto types = cache.read_transaction_types();
      bool updated = false;
      for (auto& type : types) {
        if (type.category == cat) {
          type.repetition = repetition;
          type.amount = *amount;
          if (desc_set) {
            type.description_regex = desc_regex;
          }
          if (debit_set) {
            type.debit_regex = debit_regex;
          }
          updated = true;
          std::cout << "Updated transaction " << cat << ": " << *amount
                    << " on " << repetition << std::endl;
          break;
        }
      }
      if (!updated) {
        budget::TransactionType type;
        type.category = cat;
        type.repetition = repetition;
        type.amount = *amount;
        type.description_regex = desc_regex;
        type.debit_regex = debit_regex;
        types.push_back(type);
        std::cout << "Added a new transaction " << cat << ": " << *amount
                  << " on " << repetition << std::endl;
      }
      cache.write_transaction_types(types);
#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
      budget::cli::set_category_provider([&cache]() {
        std::vector<std::string> cats;
        auto types = cache.read_transaction_types();
        cats.reserve(types.size());
        for (const auto& type : types) {
          cats.push_back(type.category);
        }
        return cats;
      });
#endif
      continue;
    }

    if (line.rfind("update theme ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("update theme ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 4) {
        std::cerr << "Usage: update theme <cat> <fg> <bg> <style>" << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      bool is_default = (cat == "default");
      if (!is_default && !is_valid_category(cat)) {
        std::cerr << "Bad category name: " << cat << std::endl;
        continue;
      }
      auto fg = parse_int(tokens[1]);
      auto bg = parse_int(tokens[2]);
      if (!fg.has_value() || !bg.has_value()) {
        std::cerr << "Color values must be integers" << std::endl;
        continue;
      }
      if (*fg < 0 || *fg > 255 || *bg < 0 || *bg > 255) {
        std::cerr << "Color values must be 0-255" << std::endl;
        continue;
      }
      std::string style = tokens[3];
      auto themes = cache.read_themes();
      std::string key = is_default ? "__default__" : cat;
      themes[key] = {*fg, *bg, style};
      cache.write_themes(themes);
      std::cout << "Updated theme " << cat << ": " << *fg << ", " << *bg << ", "
                << style << std::endl;
      continue;
    }

    if (line.rfind("update last ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("update last ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 2) {
        std::cerr << "Usage: update last <cat> <mm-dd-yyyy>" << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      auto date = budget::Date::parse_mm_dd_yyyy(tokens[1]);
      if (!date.has_value()) {
        std::cerr << "Invalid date: " << tokens[1] << std::endl;
        continue;
      }
      auto types = cache.read_transaction_types();
      bool exists = false;
      for (const auto& type : types) {
        if (type.category == cat) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        std::cerr << "Unrecognized category: " << cat << std::endl;
        continue;
      }
      auto lasts = cache.read_lasts();
      lasts[cat] = date->to_mm_dd_yyyy();
      cache.write_lasts(lasts);
      std::cout << "Updated last " << cat << ": " << date->to_mm_dd_yyyy()
                << std::endl;
      continue;
    }

    if (line.rfind("del header ", 0) == 0) {
      std::string internal =
          trim(line.substr(std::string("del header ").size()));
      if (internal.empty()) {
        std::cerr << "Usage: del header <internal>" << std::endl;
        continue;
      }
      auto map = budget::io::read_json_map(map_path);
      if (!map.has_value()) {
        std::cout << "No header map found at: " << map_path << std::endl;
        continue;
      }
      auto& m = *map;
      auto it = m.find(internal);
      if (it == m.end()) {
        std::cout << "No such header mapping: " << internal << std::endl;
        continue;
      }
      m.erase(it);
      if (!budget::io::write_json_map(map_path, m)) {
        std::cerr << "Failed to write header map: " << map_path << std::endl;
        continue;
      }
      std::cout << "Deleted header mapping: " << internal << std::endl;
      continue;
    }

    if (line.rfind("del theme ", 0) == 0) {
      std::string rest = trim(line.substr(std::string("del theme ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.empty()) {
        std::cerr << "Usage: del theme <cat|*> [-f]" << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      if (cat == "default") {
        cat = "__default__";
      }
      bool force = tokens.size() > 1 && tokens[1] == "-f";
      auto themes = cache.read_themes();
      if (cat == "*") {
        if (!force) {
          std::cout << "Delete all themes? (y/N): ";
          std::string answer;
          if (!std::getline(std::cin, answer)) {
            continue;
          }
          answer = trim(answer);
          if (answer != "y" && answer != "Y") {
            std::cout << "Aborted." << std::endl;
            continue;
          }
        }
        themes.clear();
        cache.write_themes(themes);
        std::cout << "Removed all themes" << std::endl;
        continue;
      }
      auto it = themes.find(cat);
      if (it == themes.end()) {
        std::cerr << "Theme not found: " << cat << std::endl;
        continue;
      }
      themes.erase(it);
      cache.write_themes(themes);
      std::cout << "Removed " << cat << " theme" << std::endl;
      continue;
    }

    if (line.rfind("del profile ", 0) == 0) {
      std::string name = trim(line.substr(std::string("del profile ").size()));
      if (name.empty()) {
        std::cerr << "Usage: del profile <name>" << std::endl;
        continue;
      }
      if (name == "Main") {
        std::cerr << "Profile \"Main\" cannot be deleted" << std::endl;
        continue;
      }
      auto profiles = ensure_default_profiles(
          cache.read_profiles({{"Main", "Default Profile", 0.0}}));
      auto it = std::remove_if(
          profiles.begin(), profiles.end(),
          [&](const budget::Profile& profile) { return profile.name == name; });
      if (it == profiles.end()) {
        std::cerr << "Profile not found: " << name << std::endl;
        continue;
      }
      profiles.erase(it, profiles.end());
      cache.write_profiles(profiles);
      cache.delete_profile(name);
      std::cout << "Removed profile: " << name << std::endl;
      if (active_profile == name) {
        active_profile = "Main";
        cache = budget::io::Cache("budget", active_profile);
        cache.write_session_profile(active_profile);
      }
#if defined(HAVE_READLINE) || defined(HAVE_EDITLINE)
      budget::cli::set_profile_provider([&cache]() {
        std::vector<std::string> names;
        auto profiles = ensure_default_profiles(cache.read_profiles());
        names.reserve(profiles.size());
        for (const auto& profile : profiles) {
          names.push_back(profile.name);
        }
        return names;
      });
#endif
      continue;
    }

    if (line.rfind("del exception ", 0) == 0) {
      std::string rest =
          trim(line.substr(std::string("del exception ").size()));
      auto tokens = split_tokens(rest);
      if (tokens.size() < 2) {
        std::cerr << "Usage: del exception <cat> <mm-dd-yyyy|*>" << std::endl;
        continue;
      }
      std::string cat = tokens[0];
      std::string date = tokens[1];
      auto exceptions = cache.read_exceptions();
      if (date == "*") {
        auto before = exceptions.size();
        exceptions.erase(std::remove_if(exceptions.begin(), exceptions.end(),
                                        [&](const budget::Exception& exc) {
                                          return exc.category == cat;
                                        }),
                         exceptions.end());
        if (exceptions.size() == before) {
          std::cerr << "No exceptions found for category: " << cat << std::endl;
          continue;
        }
        cache.write_exceptions(exceptions);
        std::cout << "Removed all " << cat << " exceptions" << std::endl;
        continue;
      }
      auto parsed = budget::Date::parse_mm_dd_yyyy(date);
      if (!parsed.has_value()) {
        std::cerr << "Invalid date: " << date << std::endl;
        continue;
      }
      auto before = exceptions.size();
      exceptions.erase(std::remove_if(exceptions.begin(), exceptions.end(),
                                      [&](const budget::Exception& exc) {
                                        return exc.category == cat &&
                                               exc.date == *parsed;
                                      }),
                       exceptions.end());
      if (exceptions.size() == before) {
        std::cerr << "Exception not found: " << cat << " " << date << std::endl;
        continue;
      }
      cache.write_exceptions(exceptions);
      std::cout << "Removed " << cat << " exception for " << date << std::endl;
      continue;
    }

    if (line.rfind("del transaction ", 0) == 0) {
      std::string cat =
          trim(line.substr(std::string("del transaction ").size()));
      if (cat.empty()) {
        std::cerr << "Usage: del transaction <cat>" << std::endl;
        continue;
      }
      auto types = cache.read_transaction_types();
      auto before = types.size();
      types.erase(std::remove_if(types.begin(), types.end(),
                                 [&](const budget::TransactionType& type) {
                                   return type.category == cat;
                                 }),
                  types.end());
      if (types.size() == before) {
        std::cerr << "Unrecognized category: " << cat << std::endl;
        continue;
      }
      cache.write_transaction_types(types);
      auto exceptions = cache.read_exceptions();
      exceptions.erase(std::remove_if(exceptions.begin(), exceptions.end(),
                                      [&](const budget::Exception& exc) {
                                        return exc.category == cat;
                                      }),
                       exceptions.end());
      cache.write_exceptions(exceptions);
      auto themes = cache.read_themes();
      auto it_theme = themes.find(cat);
      if (it_theme != themes.end()) {
        themes.erase(it_theme);
        cache.write_themes(themes);
      }
      std::cout << "Removed " << cat << " transaction" << std::endl;
      continue;
    }

    if (line.rfind("del last ", 0) == 0) {
      std::string cat = trim(line.substr(std::string("del last ").size()));
      if (cat.empty()) {
        std::cerr << "Usage: del last <cat|*>" << std::endl;
        continue;
      }
      auto lasts = cache.read_lasts();
      if (cat == "*") {
        lasts.clear();
        cache.write_lasts(lasts);
        std::cout << "Removed all last occurrence records" << std::endl;
        continue;
      }
      auto it = lasts.find(cat);
      if (it == lasts.end()) {
        std::cerr << "Last occurrence not found: " << cat << std::endl;
        continue;
      }
      lasts.erase(it);
      cache.write_lasts(lasts);
      std::cout << "Removed last occurrence for " << cat << std::endl;
      continue;
    }

    std::cerr << "Unrecognized command: " << line << std::endl;
  }

  return 0;
}
