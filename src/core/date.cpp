#include "date.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace budget {

namespace {

// Howard Hinnant's civil date algorithms
int days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

Date civil_from_days(int z) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1;
  unsigned m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
  return Date{y, static_cast<int>(m), static_cast<int>(d)};
}

bool parse_int(const std::string& text, size_t pos, size_t len, int& value) {
  if (pos + len > text.size()) {
    return false;
  }
  int v = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = text[pos + i];
    if (c < '0' || c > '9') {
      return false;
    }
    v = v * 10 + (c - '0');
  }
  value = v;
  return true;
}

}  // namespace

bool Date::operator==(const Date& other) const {
  return year == other.year && month == other.month && day == other.day;
}
bool Date::operator!=(const Date& other) const { return !(*this == other); }
bool Date::operator<(const Date& other) const {
  return days_since_epoch() < other.days_since_epoch();
}
bool Date::operator<=(const Date& other) const { return !(*this > other); }
bool Date::operator>(const Date& other) const {
  return days_since_epoch() > other.days_since_epoch();
}
bool Date::operator>=(const Date& other) const { return !(*this < other); }

std::string Date::to_mm_dd_yyyy() const {
  std::ostringstream out;
  out << std::setw(2) << std::setfill('0') << month << "-" << std::setw(2)
      << std::setfill('0') << day << "-" << year;
  return out.str();
}

std::string Date::to_yyyymmdd() const {
  std::ostringstream out;
  out << std::setw(4) << std::setfill('0') << year << std::setw(2)
      << std::setfill('0') << month << std::setw(2) << std::setfill('0') << day;
  return out.str();
}

int Date::weekday_index() const {
  int z = days_since_epoch();
  int idx = (z + 4) % 7;  // 1970-01-01 is Thursday (index 4)
  if (idx < 0) {
    idx += 7;
  }
  return idx;
}

std::string Date::weekday_name() const {
  static const char* names[] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};
  return names[weekday_index()];
}

Date Date::add_days(int days) const {
  return civil_from_days(days_since_epoch() + days);
}

int Date::days_since_epoch() const {
  return days_from_civil(year, static_cast<unsigned>(month),
                         static_cast<unsigned>(day));
}

int64_t Date::epoch_seconds_est_midnight() const {
  // Midnight EST = UTC + 5 hours
  constexpr int64_t offset = 5 * 3600;
  return static_cast<int64_t>(days_since_epoch()) * 86400 + offset;
}

std::optional<Date> Date::parse_mm_dd_yyyy(const std::string& text) {
  if (text.size() != 10 || text[2] != '-' || text[5] != '-') {
    return std::nullopt;
  }
  int mm = 0, dd = 0, yyyy = 0;
  if (!parse_int(text, 0, 2, mm) || !parse_int(text, 3, 2, dd) ||
      !parse_int(text, 6, 4, yyyy)) {
    return std::nullopt;
  }
  return Date{yyyy, mm, dd};
}

std::optional<Date> Date::parse_mm_dd_yyyy_slash(const std::string& text) {
  if (text.size() != 10 || text[2] != '/' || text[5] != '/') {
    return std::nullopt;
  }
  int mm = 0, dd = 0, yyyy = 0;
  if (!parse_int(text, 0, 2, mm) || !parse_int(text, 3, 2, dd) ||
      !parse_int(text, 6, 4, yyyy)) {
    return std::nullopt;
  }
  return Date{yyyy, mm, dd};
}

Date today_est() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto seconds =
      duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  constexpr int64_t offset = 5 * 3600;
  // Shift UTC to EST by subtracting 5 hours.
  int64_t est_seconds = seconds - offset;
  int64_t days = est_seconds / 86400;
  if (est_seconds < 0 && est_seconds % 86400 != 0) {
    --days;
  }
  return civil_from_days(static_cast<int>(days));
}

}  // namespace budget
