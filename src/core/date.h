#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace budget {

struct Date {
  int year = 1970;
  int month = 1;  // 1-12
  int day = 1;    // 1-31

  bool operator==(const Date& other) const;
  bool operator!=(const Date& other) const;
  bool operator<(const Date& other) const;
  bool operator<=(const Date& other) const;
  bool operator>(const Date& other) const;
  bool operator>=(const Date& other) const;

  std::string to_mm_dd_yyyy() const;
  std::string to_yyyymmdd() const;

  // 0=Sun, 1=Mon, ..., 6=Sat
  int weekday_index() const;
  std::string weekday_name() const;

  Date add_days(int days) const;
  int days_since_epoch() const;  // days since 1970-01-01
  int64_t epoch_seconds_est_midnight()
      const;  // midnight in EST as epoch seconds

  static std::optional<Date> parse_mm_dd_yyyy(const std::string& text);
  static std::optional<Date> parse_mm_dd_yyyy_slash(const std::string& text);
};

Date today_est();

}  // namespace budget
