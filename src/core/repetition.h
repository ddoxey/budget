#pragma once

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace budget {

struct RepetitionParse {
  std::string when;
  int repeater = 1;
  bool auto_flag = false;
};

class Repetition {
 public:
  static std::optional<RepetitionParse> parse(const std::string& text) {
    static const std::regex re(
        R"(^@?((Sun|Mon|Tue|Wed|Thu|Fri|Sat)|([0-9]{1,2}(,[0-9]{1,2})*))(/([0-9]+))?$)",
        std::regex_constants::icase);
    std::smatch m;
    if (!std::regex_match(text, m, re)) {
      return std::nullopt;
    }
    RepetitionParse out;
    out.auto_flag = !text.empty() && text[0] == '@';
    out.when = m[1].str();
    if (m[6].matched) {
      out.repeater = std::stoi(m[6].str());
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
  }

  const std::string& when() const { return when_; }
  int repeater() const { return repeater_; }
  bool auto_flag() const { return auto_flag_; }

  std::vector<std::string> values() const {
    std::vector<std::string> result;
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
    if (!when_.empty() && std::isdigit(static_cast<unsigned char>(when_[0]))) {
      return "%d";
    }
    return "%a";
  }

  double monthly_factor() const {
    auto vals = values();
    if (vals.size() == 1) {
      if (!vals[0].empty() &&
          std::isdigit(static_cast<unsigned char>(vals[0][0]))) {
        return 1.0 / repeater_;
      }
      return 4.0 / repeater_;
    }
    return static_cast<double>(vals.size());
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

    std::string repeats = "every";
    if (repeater_ > 1) {
      repeats = "every " + ordinal(repeater_);
    }

    if (!when_.empty() && std::isdigit(static_cast<unsigned char>(when_[0]))) {
      int day = std::stoi(values()[0]);
      std::string when = ordinal(day);
      if (repeats == "every") {
        return when + " monthly";
      }
      return repeats + " " + when;
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
};

}  // namespace budget
