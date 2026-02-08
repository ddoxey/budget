#include "holidays.h"

#include <unordered_map>
#include <unordered_set>

namespace budget {

namespace {

int days_in_month(int year, int month) {
  if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 ||
      month == 10 || month == 12) {
    return 31;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  }
  bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  return leap ? 29 : 28;
}

Date nth_weekday(int year, int month, int weekday_index, int nth) {
  Date first{year, month, 1};
  int first_wday = first.weekday_index();
  int offset = (weekday_index - first_wday + 7) % 7;
  int day = 1 + offset + (nth - 1) * 7;
  return Date{year, month, day};
}

Date last_weekday(int year, int month, int weekday_index) {
  int last_day = days_in_month(year, month);
  Date last{year, month, last_day};
  int last_wday = last.weekday_index();
  int offset = (last_wday - weekday_index + 7) % 7;
  return Date{year, month, last_day - offset};
}

void add_fixed_holiday(int year, int month, int day,
                       std::unordered_set<int>& dates) {
  Date date{year, month, day};
  dates.insert(date.days_since_epoch());
  int wday = date.weekday_index();
  if (wday == 6) {  // Saturday -> observed Friday
    Date observed = date.add_days(-1);
    dates.insert(observed.days_since_epoch());
  } else if (wday == 0) {  // Sunday -> observed Monday
    Date observed = date.add_days(1);
    dates.insert(observed.days_since_epoch());
  }
}

std::unordered_set<int> federal_holidays_for_year(int year) {
  std::unordered_set<int> dates;

  add_fixed_holiday(year, 1, 1, dates);   // New Year's Day
  add_fixed_holiday(year, 6, 19, dates);  // Juneteenth
  add_fixed_holiday(year, 7, 4, dates);   // Independence Day
  add_fixed_holiday(year, 11, 11, dates); // Veterans Day
  add_fixed_holiday(year, 12, 25, dates); // Christmas

  Date mlk = nth_weekday(year, 1, 1, 3);      // 3rd Monday Jan
  Date presidents = nth_weekday(year, 2, 1, 3); // 3rd Monday Feb
  Date memorial = last_weekday(year, 5, 1);   // last Monday May
  Date labor = nth_weekday(year, 9, 1, 1);    // 1st Monday Sep
  Date columbus = nth_weekday(year, 10, 1, 2); // 2nd Monday Oct
  Date thanksgiving = nth_weekday(year, 11, 4, 4); // 4th Thursday Nov

  dates.insert(mlk.days_since_epoch());
  dates.insert(presidents.days_since_epoch());
  dates.insert(memorial.days_since_epoch());
  dates.insert(labor.days_since_epoch());
  dates.insert(columbus.days_since_epoch());
  dates.insert(thanksgiving.days_since_epoch());

  // Include observed fixed-date holidays from next year that land in this year.
  std::unordered_set<int> next_year;
  add_fixed_holiday(year + 1, 1, 1, next_year);
  add_fixed_holiday(year + 1, 6, 19, next_year);
  add_fixed_holiday(year + 1, 7, 4, next_year);
  add_fixed_holiday(year + 1, 11, 11, next_year);
  add_fixed_holiday(year + 1, 12, 25, next_year);
  for (int key : next_year) {
    Date d = Date{1970, 1, 1}.add_days(key);
    if (d.year == year) {
      dates.insert(key);
    }
  }

  return dates;
}

const std::unordered_set<int>& cached_holidays(int year) {
  static std::unordered_map<int, std::unordered_set<int>> cache;
  auto it = cache.find(year);
  if (it != cache.end()) {
    return it->second;
  }
  auto inserted = cache.emplace(year, federal_holidays_for_year(year));
  return inserted.first->second;
}

}  // namespace

bool is_federal_holiday(const Date& date) {
  const auto& holidays = cached_holidays(date.year);
  return holidays.find(date.days_since_epoch()) != holidays.end();
}

bool is_business_day(const Date& date) {
  int wday = date.weekday_index();
  if (wday == 0 || wday == 6) {
    return false;
  }
  return !is_federal_holiday(date);
}

Date adjust_to_business_day(const Date& date) {
  Date current = date;
  while (!is_business_day(current)) {
    current = current.add_days(-1);
  }
  return current;
}

}  // namespace budget
