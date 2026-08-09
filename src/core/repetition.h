#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace budget {

enum class RepetitionKind {
  ExplicitWeekday,
  ExplicitMonthDays,
  ExplicitAnnualDate,
  CountPerWeek,
  CountPerMonth,
};

struct RepetitionParse {
  std::string when;
  int repeater = 1;
  bool auto_flag = false;
  RepetitionKind kind = RepetitionKind::ExplicitWeekday;
  int count = 0;
  int offset = 0;
  int annual_month = 0;
  int annual_day = 0;
};

class Repetition {
 public:
  static std::optional<RepetitionParse> parse(const std::string& text) {
    static const std::regex explicit_re(
        R"(^@?((Sun|Mon|Tue|Wed|Thu|Fri|Sat)|([0-9]{1,2}(,[0-9]{1,2})*))(/([0-9]+))?$)",
        std::regex_constants::icase);
    static const std::regex counted_re(
        R"(^@?([0-9]+)x(Week|Month)(\+([0-9]+))?$)",
        std::regex_constants::icase);
    static const std::regex annual_re(
        R"(^@?(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)-([0-9]{1,2})$)",
        std::regex_constants::icase);
    std::smatch m;
    RepetitionParse out;
    out.auto_flag = !text.empty() && text[0] == '@';

    if (std::regex_match(text, m, counted_re)) {
      out.when = m[1].str() + "x" + m[2].str();
      out.count = std::stoi(m[1].str());
      out.offset = m[4].matched ? std::stoi(m[4].str()) : 0;
      std::string unit = m[2].str();
      for (char& c : unit) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      out.kind = unit == "week" ? RepetitionKind::CountPerWeek
                                : RepetitionKind::CountPerMonth;
      if ((out.kind == RepetitionKind::CountPerWeek &&
           (out.count < 1 || out.count > 7)) ||
          (out.kind == RepetitionKind::CountPerMonth &&
           (out.count < 1 || out.count > 31))) {
        return std::nullopt;
      }
      return out;
    }

    if (std::regex_match(text, m, annual_re)) {
      static const std::vector<std::string> months = {
          "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      std::string month = m[1].str();
      month[0] = static_cast<char>(
          std::toupper(static_cast<unsigned char>(month[0])));
      for (size_t i = 1; i < month.size(); ++i) {
        month[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(month[i])));
      }
      auto month_it = std::find(months.begin(), months.end(), month);
      int day = std::stoi(m[2].str());
      static const int max_days[] = {31, 29, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
      if (month_it == months.end()) {
        return std::nullopt;
      }
      int month_number =
          static_cast<int>(std::distance(months.begin(), month_it)) + 1;
      if (day < 1 || day > max_days[month_number - 1]) {
        return std::nullopt;
      }
      out.kind = RepetitionKind::ExplicitAnnualDate;
      out.annual_month = month_number;
      out.annual_day = day;
      out.when = month + "-" + std::to_string(day);
      return out;
    }

    if (!std::regex_match(text, m, explicit_re)) {
      return std::nullopt;
    }
    out.when = m[1].str();
    if (m[6].matched) {
      out.repeater = std::stoi(m[6].str());
    }
    if (out.repeater <= 0) {
      return std::nullopt;
    }
    if (!out.when.empty() &&
        std::isdigit(static_cast<unsigned char>(out.when[0]))) {
      out.kind = RepetitionKind::ExplicitMonthDays;
      std::vector<std::string> seen;
      size_t start = 0;
      while (start < out.when.size()) {
        size_t comma = out.when.find(',', start);
        std::string token =
            out.when.substr(start, comma == std::string::npos
                                       ? std::string::npos
                                       : comma - start);
        int day = std::stoi(token);
        if (day < 1 || day > 31) {
          return std::nullopt;
        }
        if (std::find(seen.begin(), seen.end(), token) != seen.end()) {
          return std::nullopt;
        }
        seen.push_back(token);
        if (comma == std::string::npos) {
          break;
        }
        start = comma + 1;
      }
    } else {
      out.kind = RepetitionKind::ExplicitWeekday;
    }
    return out;
  }

  explicit Repetition(const std::string& text) {
    auto parsed = parse(text);
    if (!parsed.has_value()) {
      throw std::invalid_argument("Invalid repetition string: " + text);
    }
    when_ = parsed->when;
    repeater_ = parsed->repeater;
    auto_flag_ = parsed->auto_flag;
    kind_ = parsed->kind;
    count_ = parsed->count;
    offset_ = parsed->offset;
    annual_month_ = parsed->annual_month;
    annual_day_ = parsed->annual_day;
  }

  const std::string& when() const { return when_; }
  int repeater() const { return repeater_; }
  bool auto_flag() const { return auto_flag_; }
  RepetitionKind kind() const { return kind_; }
  int count() const { return count_; }
  int offset() const { return offset_; }
  int annual_month() const { return annual_month_; }
  int annual_day() const { return annual_day_; }
  bool is_count_pattern() const {
    return kind_ == RepetitionKind::CountPerWeek ||
           kind_ == RepetitionKind::CountPerMonth;
  }

  std::vector<std::string> values() const {
    std::vector<std::string> result;
    if (kind_ == RepetitionKind::CountPerWeek ||
        kind_ == RepetitionKind::CountPerMonth) {
      return result;
    }
    if (!when_.empty() && std::isdigit(static_cast<unsigned char>(when_[0]))) {
      size_t start = 0;
      while (start < when_.size()) {
        size_t comma = when_.find(',', start);
        std::string token =
            when_.substr(start, comma == std::string::npos ? std::string::npos
                                                           : comma - start);
        if (token.size() == 1) {
          token = "0" + token;
        }
        result.push_back(token);
        if (comma == std::string::npos) {
          break;
        }
        start = comma + 1;
      }
      return result;
    }
    std::string day = when_;
    if (!day.empty()) {
      day[0] =
          static_cast<char>(std::toupper(static_cast<unsigned char>(day[0])));
      for (size_t i = 1; i < day.size(); ++i) {
        day[i] =
            static_cast<char>(std::tolower(static_cast<unsigned char>(day[i])));
      }
      result.push_back(day);
    }
    return result;
  }

  std::string field() const {
    if (kind_ == RepetitionKind::ExplicitMonthDays ||
        kind_ == RepetitionKind::CountPerMonth) {
      return "%d";
    }
    return "%a";
  }

  std::vector<int> positions_in_week() const {
    std::vector<int> positions;
    if (kind_ != RepetitionKind::CountPerWeek) {
      return positions;
    }
    positions.reserve(static_cast<size_t>(count_));
    for (int i = 0; i < count_; ++i) {
      int base = (i * 7 + count_ - 1) / count_;
      positions.push_back((base + offset_) % 7);
    }
    std::sort(positions.begin(), positions.end());
    return positions;
  }

  std::vector<int> positions_in_month(int days_in_month) const {
    std::vector<int> positions;
    if (kind_ != RepetitionKind::CountPerMonth || days_in_month <= 0) {
      return positions;
    }
    positions.reserve(static_cast<size_t>(count_));
    int normalized_offset = offset_ % days_in_month;
    for (int i = 0; i < count_; ++i) {
      int base = (i * (days_in_month - 1)) / count_;
      positions.push_back(((base + normalized_offset) % days_in_month) + 1);
    }
    std::sort(positions.begin(), positions.end());
    return positions;
  }

  double monthly_factor() const {
    if (kind_ == RepetitionKind::ExplicitAnnualDate) {
      return 1.0 / 12.0;
    }
    if (kind_ == RepetitionKind::CountPerWeek) {
      return 4.0 * count_;
    }
    if (kind_ == RepetitionKind::CountPerMonth) {
      return static_cast<double>(count_);
    }
    auto vals = values();
    if (vals.size() == 1) {
      if (!vals[0].empty() &&
          std::isdigit(static_cast<unsigned char>(vals[0][0]))) {
        return 1.0 / repeater_;
      }
      return 4.0 / repeater_;
    }
    return static_cast<double>(vals.size()) / repeater_;
  }

  std::string to_string() const {
    auto ordinal = [](int n) {
      int mod100 = n % 100;
      if (mod100 >= 11 && mod100 <= 13) {
        return std::to_string(n) + "th";
      }
      switch (n % 10) {
        case 1:
          return std::to_string(n) + "st";
        case 2:
          return std::to_string(n) + "nd";
        case 3:
          return std::to_string(n) + "rd";
        default:
          return std::to_string(n) + "th";
      }
    };
    auto count_label = [](int n) {
      switch (n) {
        case 1:
          return std::string("Once");
        case 2:
          return std::string("Twice");
        default:
          return std::to_string(n) + " times";
      }
    };
    auto weekday_name = [](int index) {
      static const char* names[] = {"Sun", "Mon", "Tue", "Wed",
                                    "Thu", "Fri", "Sat"};
      if (index < 0 || index > 6) {
        return std::string("?");
      }
      return std::string(names[index]);
    };

    std::string repeats = "every";
    if (repeater_ > 1) {
      repeats = "every " + ordinal(repeater_);
    }

    if (kind_ == RepetitionKind::CountPerWeek ||
        kind_ == RepetitionKind::CountPerMonth) {
      std::string unit = kind_ == RepetitionKind::CountPerWeek ? "week" : "month";
      std::string result = count_label(count_) + " a " + unit;
      if (offset_ > 0) {
        if (kind_ == RepetitionKind::CountPerWeek) {
          auto positions = positions_in_week();
          if (!positions.empty()) {
            result += " (starting on " + weekday_name(positions.front()) + ")";
          }
        } else {
          result += " (starting on " + ordinal(offset_ + 1) + ")";
        }
      }
      return result;
    }

    if (kind_ == RepetitionKind::ExplicitAnnualDate) {
      return "annually on " + when_.substr(0, 3) + " " +
             ordinal(annual_day_);
    }

    if (!when_.empty() && std::isdigit(static_cast<unsigned char>(when_[0]))) {
      auto vals = values();
      std::string when;
      for (size_t i = 0; i < vals.size(); ++i) {
        if (i > 0) {
          when += ", ";
        }
        when += ordinal(std::stoi(vals[i]));
      }
      if (repeats == "every") {
        return when + " monthly";
      }
      return repeats + " month on the " + when;
    }

    std::string when = values().empty() ? when_ : values()[0];
    if (repeats == "every") {
      return repeats + " " + when;
    }
    return repeats + " " + when;
  }

 private:
  std::string when_;
  int repeater_ = 1;
  bool auto_flag_ = false;
  RepetitionKind kind_ = RepetitionKind::ExplicitWeekday;
  int count_ = 0;
  int offset_ = 0;
  int annual_month_ = 0;
  int annual_day_ = 0;
};

}  // namespace budget
