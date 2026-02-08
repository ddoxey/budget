#pragma once

#include <algorithm>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace budget {

class Money {
 public:
  static bool matches(const std::string& text) {
    if (text.empty()) {
      return false;
    }
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
      if (c != ',') {
        cleaned.push_back(c);
      }
    }
    return std::regex_match(cleaned, regex());
  }

  static std::optional<double> parse(const std::string& text) {
    if (text.empty()) {
      return std::nullopt;
    }
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
      if (c != ',') {
        cleaned.push_back(c);
      }
    }
    if (cleaned.find('.') == std::string::npos) {
      cleaned += ".00";
    }
    if (!std::regex_match(cleaned, regex())) {
      return std::nullopt;
    }
    std::string numeric;
    numeric.reserve(cleaned.size());
    for (char c : cleaned) {
      if (c != '$') {
        numeric.push_back(c);
      }
    }
    try {
      return std::stod(numeric);
    } catch (...) {
      return std::nullopt;
    }
  }

  explicit Money(double amount, std::string symbol = "")
      : amount_(amount), symbol_(std::move(symbol)) {}

  explicit Money(const std::string& text, std::string symbol = "")
      : symbol_(std::move(symbol)) {
    auto parsed = parse(text);
    if (!parsed.has_value()) {
      throw std::invalid_argument(text + " should resemble: $1.00");
    }
    amount_ = *parsed;
  }

  double value() const { return amount_; }

  std::string str() const {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(2) << amount_;
    std::string s = out.str();
    auto dot = s.find('.');
    std::string dollars = s.substr(0, dot);
    std::string cents = s.substr(dot + 1);
    bool negative = !dollars.empty() && dollars[0] == '-';
    std::string digits = negative ? dollars.substr(1) : dollars;
    std::string with_commas;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
      if (count == 3) {
        with_commas.push_back(',');
        count = 0;
      }
      with_commas.push_back(*it);
      ++count;
    }
    std::reverse(with_commas.begin(), with_commas.end());
    if (negative) {
      with_commas.insert(with_commas.begin(), '-');
    }
    return symbol_ + with_commas + "." + cents;
  }

 private:
  double amount_ = 0.0;
  std::string symbol_;

  static const std::regex& regex() {
    static const std::regex re(R"(^([$]?[-]?|[-]?[$]?)[0-9]+([.][0-9]+)?$)");
    return re;
  }
};

}  // namespace budget
