#include "budget.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
#include <stdexcept>

#include "holidays.h"
namespace budget {

namespace {

int month_index(const Date& date) {
  return date.year * 12 + (date.month - 1);
}

Date first_day_of_month_from_index(int index) {
  int year = index / 12;
  int month = index % 12;
  if (month < 0) {
    month += 12;
    year -= 1;
  }
  return Date{year, month + 1, 1};
}

int days_in_month(int year, int month) {
  static const int base_days[] = {31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
  if (month == 2) {
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return base_days[month - 1];
}

bool date_matches_repetition(const Repetition& repetition, const Date& date) {
  if (repetition.kind() == RepetitionKind::ExplicitAnnualDate) {
    return date.month == repetition.annual_month() &&
           date.day == repetition.annual_day();
  }
  if (repetition.kind() == RepetitionKind::CountPerWeek) {
    auto positions = repetition.positions_in_week();
    return std::find(positions.begin(), positions.end(),
                     date.weekday_index()) != positions.end();
  }
  if (repetition.kind() == RepetitionKind::CountPerMonth) {
    auto positions =
        repetition.positions_in_month(days_in_month(date.year, date.month));
    return std::find(positions.begin(), positions.end(), date.day) !=
           positions.end();
  }
  std::string value = repetition.field() == "%d"
                          ? date.to_mm_dd_yyyy().substr(3, 2)
                          : date.weekday_name();
  auto vals = repetition.values();
  return std::find(vals.begin(), vals.end(), value) != vals.end();
}

bool is_active_month_from_anchor(const Repetition& repetition, const Date& anchor,
                                 const Date& date) {
  if (repetition.kind() != RepetitionKind::ExplicitMonthDays) {
    return true;
  }
  int delta = month_index(date) - month_index(anchor);
  return delta >= 0 && delta % repetition.repeater() == 0;
}

bool is_active_month_for_last(const Repetition& repetition, const Date& now,
                              const Date& date) {
  if (repetition.kind() != RepetitionKind::ExplicitMonthDays) {
    return true;
  }
  int delta = month_index(now) - month_index(date);
  return delta >= 0 && delta % repetition.repeater() == repetition.repeater() - 1;
}

bool regex_match_ci(const std::string& text, const std::string& pattern) {
  if (pattern.empty()) {
    return false;
  }
  try {
    std::regex re(pattern, std::regex_constants::icase);
    return std::regex_search(text, re);
  } catch (...) {
    return false;
  }
}

bool matches_conditions(const Transaction& transaction,
                        const TransactionType& trans_type) {
  int hit_count = 0;
  int condition_count = 0;

  auto it_desc = transaction.fields.find("description");
  if (!trans_type.description_regex.empty() &&
      it_desc != transaction.fields.end()) {
    condition_count += 1;
    if (regex_match_ci(it_desc->second, trans_type.description_regex)) {
      hit_count += 1;
    }
  }

  auto it_debit = transaction.fields.find("debit");
  if (!trans_type.debit_regex.empty() && it_debit != transaction.fields.end()) {
    condition_count += 1;
    if (regex_match_ci(it_debit->second, trans_type.debit_regex)) {
      hit_count += 1;
    }
  }

  return hit_count > 0 && hit_count == condition_count;
}

std::optional<std::string> categorize_transaction(
    const Transaction& transaction, const std::vector<TransactionType>& types) {
  auto it = transaction.fields.find("cat");
  if (it != transaction.fields.end()) {
    return it->second;
  }
  for (const auto& type : types) {
    if (matches_conditions(transaction, type)) {
      return type.category;
    }
  }
  return std::nullopt;
}

Date compute_last(const TransactionType& trans_type, const Date& now) {
  Repetition repetition(trans_type.repetition);
  if (repetition.kind() == RepetitionKind::ExplicitAnnualDate) {
    for (int year = now.year; year >= now.year - 8; --year) {
      if (repetition.annual_day() >
          days_in_month(year, repetition.annual_month())) {
        continue;
      }
      Date scheduled{year, repetition.annual_month(), repetition.annual_day()};
      Date occurrence = repetition.auto_flag()
                            ? adjust_to_business_day(scheduled)
                            : scheduled;
      if (occurrence <= now) {
        return occurrence;
      }
    }
    throw std::runtime_error("failed to compute last occurrence of " +
                             trans_type.category);
  }
  if (repetition.kind() == RepetitionKind::ExplicitMonthDays ||
      repetition.kind() == RepetitionKind::CountPerMonth) {
    for (int month_delta = 0; month_delta <= repetition.repeater() + 12;
         ++month_delta) {
      Date month_start =
          first_day_of_month_from_index(month_index(Date{now.year, now.month, 1}) -
                                        month_delta);
      if (!is_active_month_for_last(repetition, now, month_start)) {
        continue;
      }

      std::vector<int> month_days;
      if (repetition.kind() == RepetitionKind::CountPerMonth) {
        month_days =
            repetition.positions_in_month(days_in_month(month_start.year,
                                                        month_start.month));
      } else {
        auto vals = repetition.values();
        month_days.reserve(vals.size());
        for (const auto& value : vals) {
          month_days.push_back(std::stoi(value));
        }
      }

      for (auto it = month_days.rbegin(); it != month_days.rend(); ++it) {
        if (*it > days_in_month(month_start.year, month_start.month)) {
          continue;
        }
        Date candidate{month_start.year, month_start.month, *it};
        if (candidate <= now) {
          return repetition.auto_flag() ? adjust_to_business_day(candidate)
                                        : candidate;
        }
      }
    }
    throw std::runtime_error("failed to compute last occurrence of " +
                             trans_type.category);
  }

  int max_days_ago = 365;
  if (repetition.kind() == RepetitionKind::ExplicitWeekday) {
    max_days_ago = 1 + repetition.repeater() * 7;
  } else if (repetition.kind() == RepetitionKind::CountPerWeek) {
    max_days_ago = 14;
  }

  std::vector<Date> dates;
  for (int i = 0; i <= max_days_ago; ++i) {
    Date date = now.add_days(-i);
    if (date_matches_repetition(repetition, date)) {
      dates.push_back(date);
    }
  }

  std::vector<Date> filtered;
  filtered.reserve(dates.size());
  for (size_t i = 0; i < dates.size(); ++i) {
    if ((i + 1) % repetition.repeater() == 0) {
      filtered.push_back(dates[i]);
    }
  }

  if (filtered.empty()) {
    throw std::runtime_error("failed to compute last occurrence of " +
                             trans_type.category);
  }
  if (repetition.auto_flag()) {
    return adjust_to_business_day(filtered.front());
  }
  return filtered.front();
}

struct LastsResult {
  std::unordered_map<std::string, Date> last_for;
  std::unordered_map<std::string, Date> observed_for;
  std::unordered_map<std::string, Date> schedule_anchor_for;
  std::unordered_map<std::string, std::string> source_for;
};

Date scheduled_date_from_occurrence(const Repetition& repetition,
                                    const Date& occurrence) {
  if (!repetition.auto_flag()) {
    return occurrence;
  }
  for (int offset = 0; offset <= 7; ++offset) {
    Date candidate = occurrence.add_days(offset);
    if (date_matches_repetition(repetition, candidate) &&
        adjust_to_business_day(candidate) == occurrence) {
      return candidate;
    }
  }
  return occurrence;
}

int early_payment_grace_days(const Repetition& repetition,
                             const Date& occurrence) {
  Date scheduled = scheduled_date_from_occurrence(repetition, occurrence);
  int weekday_matches = 0;
  for (int days_ago = 1; days_ago <= 2000; ++days_ago) {
    Date candidate = scheduled.add_days(-days_ago);
    if (!date_matches_repetition(repetition, candidate)) {
      continue;
    }
    if (repetition.kind() == RepetitionKind::ExplicitMonthDays) {
      int month_delta = month_index(scheduled) - month_index(candidate);
      if (month_delta % repetition.repeater() != 0) {
        continue;
      }
    }
    if (repetition.kind() == RepetitionKind::ExplicitWeekday) {
      ++weekday_matches;
      if (weekday_matches % repetition.repeater() != 0) {
        continue;
      }
    }
    int interval_days = scheduled.days_since_epoch() -
                        candidate.days_since_epoch();
    return std::max(0, interval_days / 2 - 1);
  }
  int interval_days = 1;
  return std::max(0, interval_days / 2 - 1);
}

std::optional<Date> transaction_posting_date(const Transaction& transaction) {
  static const char* keys[] = {"posting_date", "posting date"};
  for (const char* key : keys) {
    auto it = transaction.fields.find(key);
    if (it == transaction.fields.end()) {
      continue;
    }
    auto parsed = Date::parse_mm_dd_yyyy_slash(it->second);
    if (parsed.has_value()) {
      return parsed;
    }
  }
  return std::nullopt;
}

bool is_history_source(const std::string& source) {
  return source == "history" || source == "history-overdue";
}

bool is_overdue_source(const std::string& source) {
  return source == "history-overdue" || source == "computed-overdue";
}

LastsResult find_lasts(const std::vector<Transaction>& history,
                       const std::vector<TransactionType>& transaction_types,
                       const std::vector<Exception>& exceptions,
                       const Date& now) {
  LastsResult result;

  for (const auto& transaction : history) {
    auto category = categorize_transaction(transaction, transaction_types);
    if (!category.has_value()) {
      continue;
    }
    auto it = transaction.fields.find("transaction_date");
    if (it == transaction.fields.end()) {
      continue;
    }
    auto parsed = Date::parse_mm_dd_yyyy_slash(it->second);
    if (!parsed.has_value()) {
      continue;
    }
    auto existing = result.last_for.find(*category);
    if (existing != result.last_for.end() && existing->second >= *parsed) {
      continue;
    }
    result.last_for[*category] = *parsed;
    result.observed_for[*category] =
        transaction_posting_date(transaction).value_or(*parsed);
    result.schedule_anchor_for[*category] = *parsed;
    std::string source = "history";
    auto source_it = transaction.fields.find("last_source");
    if (source_it != transaction.fields.end() && !source_it->second.empty()) {
      source = source_it->second;
    }
    result.source_for[*category] = source;
  }

  for (const auto& type : transaction_types) {
    auto last_it = result.last_for.find(type.category);
    if (last_it == result.last_for.end()) {
      continue;
    }
    auto source_it = result.source_for.find(type.category);
    if (source_it == result.source_for.end() ||
        !is_history_source(source_it->second)) {
      continue;
    }

    Date scheduled_last = compute_last(type, now);
    auto observed_it = result.observed_for.find(type.category);
    Date observed = observed_it != result.observed_for.end()
                        ? observed_it->second
                        : last_it->second;
    Date earliest_on_time = adjust_to_business_day(scheduled_last);
    Repetition repetition(type.repetition);
    Date earliest_acceptable =
        earliest_on_time.add_days(
            -early_payment_grace_days(repetition, scheduled_last));
    if (observed < earliest_acceptable) {
      source_it->second = "history-overdue";
    } else if (last_it->second < scheduled_last) {
      result.schedule_anchor_for[type.category] = scheduled_last;
    }
  }

  if (result.last_for.size() != transaction_types.size()) {
    for (const auto& type : transaction_types) {
      if (result.last_for.find(type.category) == result.last_for.end()) {
        Date computed_last = compute_last(type, now);
        result.last_for[type.category] = computed_last;
        result.schedule_anchor_for[type.category] = computed_last;
        result.source_for[type.category] =
            (computed_last < now) ? "computed-overdue" : "computed";
      }
    }
  }

  for (const auto& exception : exceptions) {
    if (exception.date <= now) {
      auto it = result.last_for.find(exception.category);
      auto source_it = result.source_for.find(exception.category);
      if (it == result.last_for.end() || it->second < exception.date ||
          (it->second == exception.date &&
           (source_it == result.source_for.end() ||
            source_it->second != "exception"))) {
        result.last_for[exception.category] = exception.date;
        result.schedule_anchor_for[exception.category] = exception.date;
        result.source_for[exception.category] = "exception";
      }
    }
  }

  return result;
}

std::vector<Date> build_date_list(const std::string& repetition,
                                  const Date& now, const Date& from_date,
                                  int day_span) {
  Repetition repeat(repetition);

  Date to_date = now.add_days(day_span + 1);
  if (to_date < from_date) {
    throw std::runtime_error("to_date precedes from_date");
  }

  std::vector<Date> dates;
  dates.reserve(static_cast<size_t>(
      std::max(0, to_date.days_since_epoch() - from_date.days_since_epoch())));

  int occurrence_count = 0;
  for (Date date = from_date.add_days(1); date <= to_date;
       date = date.add_days(1)) {
    if (repeat.kind() == RepetitionKind::ExplicitMonthDays &&
        !is_active_month_from_anchor(repeat, from_date, date)) {
      continue;
    }
    if (date_matches_repetition(repeat, date)) {
      occurrence_count += 1;
      if (date < now) {
        continue;
      }
      if (repeat.kind() != RepetitionKind::ExplicitWeekday ||
          repeat.repeater() == 1 ||
          occurrence_count % repeat.repeater() == 0) {
        dates.push_back(date);
      }
    }
  }

  return dates;
}

double resolve_event_amount(
    const std::unordered_map<std::string, double>& exception_for,
    const TransactionType& trans_type, const Date& scheduled,
    const Date& adjusted) {
  double amount = trans_type.amount;
  auto key_adjusted = adjusted.to_mm_dd_yyyy() + ":" + trans_type.category;
  auto key_scheduled = scheduled.to_mm_dd_yyyy() + ":" + trans_type.category;
  auto ex_it = exception_for.find(key_adjusted);
  if (ex_it != exception_for.end()) {
    amount = ex_it->second;
  } else {
    auto ex_sched = exception_for.find(key_scheduled);
    if (ex_sched != exception_for.end()) {
      amount = ex_sched->second;
    }
  }
  return amount;
}

double overdue_catchup_amount(
    const std::unordered_map<std::string, double>& exception_for,
    const TransactionType& trans_type, const Date& last_occurrence,
    const Date& now, bool include_last_occurrence) {
  Repetition repetition(trans_type.repetition);
  double total = 0.0;
  if (include_last_occurrence) {
    total += resolve_event_amount(exception_for, trans_type, last_occurrence,
                                  last_occurrence);
  }
  auto dates = build_date_list(
      trans_type.repetition, last_occurrence, last_occurrence,
      now.days_since_epoch() - last_occurrence.days_since_epoch());
  for (const auto& date : dates) {
    Date adjusted =
        repetition.auto_flag() ? adjust_to_business_day(date) : date;
    if (adjusted <= last_occurrence || adjusted > now) {
      continue;
    }
    total += resolve_event_amount(exception_for, trans_type, date, adjusted);
  }
  return total;
}

}  // namespace

Event::Event(const EventRecord& record, double& balance_ref)
    : category_(record.category),
      amount_(record.amount),
      date_(record.date),
      yyyymmdd_(record.yyyymmdd),
      epoch_(record.epoch) {
  balance_ref += amount_;
  balance_ = balance_ref;
}

Chokepoint::Chokepoint(const Event& event)
    : date_(event.get_date()),
      balance_(event.get_balance()),
      timestamp_(event.get_epoch()) {}

ChokepointList::ChokepointList(
    const std::vector<TransactionType>& transaction_types,
    const std::vector<Event>& events) {
  double major_expense = 0.0;
  double major_income = 0.0;
  std::string major_expense_cat;
  std::string major_income_cat;
  int major_expense_count = 0;

  for (const auto& trans : transaction_types) {
    if (trans.amount < major_expense) {
      major_expense = trans.amount;
      major_expense_cat = trans.category;
    } else if (trans.amount > major_income) {
      major_income = trans.amount;
      major_income_cat = trans.category;
    }
  }

  double minimum_balance = 999999.0;
  std::optional<Event> minimum_event;

  for (size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];
    if (event.get_amount() == major_expense) {
      major_expense_count += 1;
    } else if (event.get_amount() == major_income) {
      if (major_expense_count > 0 && i > 0) {
        const auto& last_event = events[i - 1];
        if (minimum_balance > 0 && minimum_balance > last_event.get_balance()) {
          minimum_balance = last_event.get_balance();
          minimum_event = last_event;
        }
        chokepoints_.emplace_back(last_event);
        datapoints_.push_back({static_cast<double>(last_event.get_epoch()),
                               last_event.get_balance()});
      }
      major_expense_count = 0;
    }
  }

  if (!events.empty() && minimum_balance > events[0].get_balance()) {
    minimum_balance = events[0].get_balance();
    minimum_event = events[0];
  }

  if (minimum_event.has_value()) {
    minimum_ = Chokepoint(*minimum_event);
  }
}

std::optional<Date> ChokepointList::crash_date() const {
  if (datapoints_.size() < 2) {
    return std::nullopt;
  }
  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_xx = 0.0;
  double sum_xy = 0.0;
  const double n = static_cast<double>(datapoints_.size());

  for (const auto& p : datapoints_) {
    sum_x += p.first;
    sum_y += p.second;
    sum_xx += p.first * p.first;
    sum_xy += p.first * p.second;
  }

  double denom = (n * sum_xx - sum_x * sum_x);
  if (denom == 0.0) {
    return std::nullopt;
  }
  double slope = (n * sum_xy - sum_x * sum_y) / denom;
  double intercept = (sum_y - slope * sum_x) / n;
  if (intercept < 0.0 || slope == 0.0) {
    return std::nullopt;
  }
  double x_intercept = -intercept / slope;
  int64_t seconds = static_cast<int64_t>(std::llround(x_intercept));
  int64_t days = seconds / 86400;
  Date epoch{1970, 1, 1};
  return epoch.add_days(static_cast<int>(days));
}

Budget::Budget(double balance, std::vector<TransactionType> transaction_types,
               std::vector<Exception> exceptions,
               std::vector<Transaction> history, int days, int days_offset,
               std::optional<Date> now_override)
    : transaction_types_(std::move(transaction_types)),
      days_(days),
      balance_(balance) {
  Date now = now_override.has_value() ? *now_override : today_est();
  if (days_offset > 0) {
    now = now.add_days(days_offset);
  }

  auto lasts = find_lasts(history, transaction_types_, exceptions, now);
  last_occurrence_of_ = lasts.last_for;
  schedule_anchor_of_ = lasts.schedule_anchor_for;
  last_source_of_ = lasts.source_for;

  for (const auto& type : transaction_types_) {
    auto source_it = lasts.source_for.find(type.category);
    if (source_it != lasts.source_for.end() &&
        is_history_source(source_it->second)) {
      continue;
    }
    if (source_it != lasts.source_for.end() &&
        source_it->second == "exception") {
      continue;
    }
    Repetition rep(type.repetition);
    if (rep.repeater() > 1) {
      std::cerr << "Warning: last occurrence for \"" << type.category
                << "\" computed from repetition (every " << rep.repeater()
                << "). Consider adding an exception record or a regex match "
                   "for description and/or amount on the transaction type."
                << std::endl;
    }
  }

  std::unordered_map<std::string, double> exception_for;
  exception_for.reserve(exceptions.size());
  for (const auto& exc : exceptions) {
    exception_for[exc.date.to_mm_dd_yyyy() + ":" + exc.category] = exc.amount;
  }

  std::vector<EventRecord> event_records;
  for (const auto& trans_type : transaction_types_) {
    auto it = last_occurrence_of_.find(trans_type.category);
    if (it == last_occurrence_of_.end()) {
      continue;
    }
    Repetition repetition(trans_type.repetition);
    auto anchor_it = schedule_anchor_of_.find(trans_type.category);
    Date schedule_anchor = anchor_it != schedule_anchor_of_.end()
                               ? anchor_it->second
                               : it->second;
    auto source_it = last_source_of_.find(trans_type.category);
    std::string source =
        source_it != last_source_of_.end() ? source_it->second : "";
    bool overdue = is_overdue_source(source);
    if (overdue) {
      double catchup = overdue_catchup_amount(
          exception_for, trans_type, schedule_anchor, now,
          source == "computed-overdue");
      if (catchup != 0.0) {
        EventRecord record;
        record.category = trans_type.category;
        record.amount = catchup;
        record.date = now;
        record.yyyymmdd = now.to_yyyymmdd();
        record.epoch = now.epoch_seconds_est_midnight();
        event_records.push_back(record);
      }
    }
    auto dates =
        build_date_list(trans_type.repetition, now, schedule_anchor, days);
    for (const auto& date : dates) {
      Date adjusted =
          repetition.auto_flag() ? adjust_to_business_day(date) : date;
      if (adjusted <= it->second) {
        continue;
      }
      if (overdue && adjusted <= now) {
        continue;
      }
      double amount =
          resolve_event_amount(exception_for, trans_type, date, adjusted);
      if (amount == 0.0) {
        continue;
      }
      EventRecord record;
      record.category = trans_type.category;
      record.amount = amount;
      record.date = adjusted;
      record.yyyymmdd = adjusted.to_yyyymmdd();
      record.epoch = adjusted.epoch_seconds_est_midnight();
      event_records.push_back(record);
    }
  }

  std::sort(event_records.begin(), event_records.end(),
            [](const EventRecord& a, const EventRecord& b) {
              if (a.epoch == b.epoch) {
                return a.amount > b.amount;
              }
              return a.epoch < b.epoch;
            });

  double bal = balance_;
  events_.reserve(event_records.size());
  for (const auto& rec : event_records) {
    events_.emplace_back(rec, bal);
  }
  balance_ = bal;
}

std::unordered_map<std::string, double> Budget::totals() {
  if (!totals_.empty()) {
    return totals_;
  }
  for (const auto& event : events_) {
    totals_[event.get_category()] += event.get_amount();
  }
  return totals_;
}

ChokepointList Budget::chokepoints() {
  if (!chokepoints_.has_value()) {
    chokepoints_ = ChokepointList(transaction_types_, events_);
  }
  return *chokepoints_;
}

std::unordered_map<std::string, std::string> Budget::last_occurrences() const {
  std::unordered_map<std::string, std::string> result;
  for (const auto& pair : last_occurrence_of_) {
    result[pair.first] = pair.second.to_mm_dd_yyyy();
  }
  return result;
}

std::unordered_map<std::string, std::string> Budget::last_occurrence_sources()
    const {
  return last_source_of_;
}

std::unordered_map<std::string, LastOccurrence>
Budget::last_occurrence_details() const {
  std::unordered_map<std::string, LastOccurrence> result;
  for (const auto& pair : last_occurrence_of_) {
    LastOccurrence occurrence;
    occurrence.date = pair.second.to_mm_dd_yyyy();
    auto source_it = last_source_of_.find(pair.first);
    occurrence.source =
        source_it != last_source_of_.end() ? source_it->second : "unknown";
    result[pair.first] = occurrence;
  }
  return result;
}

std::vector<Event> Budget::events_by_category(
    const std::string& category) const {
  std::vector<Event> result;
  for (const auto& event : events_) {
    if (event.get_category() == category) {
      result.push_back(event);
    }
  }
  return result;
}

std::vector<Event> Budget::events_by_date(
    const std::string& date_mm_dd_yyyy) const {
  std::vector<Event> result;
  for (const auto& event : events_) {
    if (event.get_date().to_mm_dd_yyyy() == date_mm_dd_yyyy) {
      result.push_back(event);
    }
  }
  return result;
}

std::vector<Event> Budget::events_by_amount(double amount) const {
  std::vector<Event> result;
  for (const auto& event : events_) {
    if (event.get_amount() == amount) {
      result.push_back(event);
    }
  }
  return result;
}

std::vector<Exception> filter_exceptions(
    const std::vector<Exception>& exceptions, const Date& now) {
  std::unordered_map<std::string, Exception> latest_expired;
  std::vector<Exception> future;

  for (const auto& exc : exceptions) {
    if (exc.date <= now) {
      auto it = latest_expired.find(exc.category);
      if (it == latest_expired.end() || it->second.date < exc.date) {
        latest_expired[exc.category] = exc;
      }
    } else {
      future.push_back(exc);
    }
  }

  std::vector<Exception> result;
  result.reserve(future.size() + latest_expired.size());
  result.insert(result.end(), future.begin(), future.end());
  for (const auto& pair : latest_expired) {
    result.push_back(pair.second);
  }
  return result;
}

}  // namespace budget
