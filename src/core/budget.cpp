#include "budget.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
#include <stdexcept>

#include "holidays.h"
namespace budget {

namespace {

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
  int max_days_ago = 365;
  Repetition repetition(trans_type.repetition);
  if (repetition.field() == "%d") {
    max_days_ago = 1 + repetition.repeater() * 31;
  } else if (repetition.field() == "%a") {
    max_days_ago = 1 + repetition.repeater() * 7;
  }

  Date from_date = now;
  Date to_date = now.add_days(-max_days_ago);

  std::vector<Date> dates;
  for (int i = 0; i <= max_days_ago; ++i) {
    Date date = from_date.add_days(-i);
    std::string value = repetition.field() == "%d"
                            ? date.to_mm_dd_yyyy().substr(3, 2)
                            : date.weekday_name();
    auto vals = repetition.values();
    if (std::find(vals.begin(), vals.end(), value) != vals.end()) {
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
  std::unordered_map<std::string, bool> from_history;
  std::unordered_map<std::string, bool> from_exception;
};

LastsResult find_lasts(const std::vector<Transaction>& history,
                       const std::vector<TransactionType>& transaction_types,
                       const std::vector<Exception>& exceptions,
                       const Date& now) {
  LastsResult result;

  for (const auto& transaction : history) {
    if (result.last_for.size() == transaction_types.size()) {
      break;
    }
    auto category = categorize_transaction(transaction, transaction_types);
    if (!category.has_value()) {
      continue;
    }
    if (result.last_for.find(*category) != result.last_for.end()) {
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
    result.last_for[*category] = *parsed;
    result.from_history[*category] = true;
  }

  if (result.last_for.size() != transaction_types.size()) {
    for (const auto& type : transaction_types) {
      if (result.last_for.find(type.category) == result.last_for.end()) {
        result.last_for[type.category] = compute_last(type, now);
      }
    }
  }

  for (const auto& exception : exceptions) {
    if (exception.date < now) {
      auto it = result.last_for.find(exception.category);
      if (it == result.last_for.end() || it->second < exception.date) {
        result.last_for[exception.category] = exception.date;
        result.from_exception[exception.category] = true;
      }
    }
  }

  return result;
}

std::vector<Date> build_date_list(const std::string& repetition,
                                  const Date& now, const Date& from_date,
                                  int day_span) {
  Repetition repeat(repetition);

  int days_ago = now.days_since_epoch() - from_date.days_since_epoch();
  int days = day_span + days_ago;

  Date to_date = now.add_days(day_span + 1);
  if (to_date < from_date) {
    throw std::runtime_error("to_date precedes from_date");
  }

  std::vector<Date> dates;
  dates.reserve(static_cast<size_t>(std::max(0, days)));

  int occurrence_count = 0;
  for (Date date = from_date.add_days(1); date <= to_date;
       date = date.add_days(1)) {
    std::string value = repeat.field() == "%d"
                            ? date.to_mm_dd_yyyy().substr(3, 2)
                            : date.weekday_name();
    auto vals = repeat.values();
    if (std::find(vals.begin(), vals.end(), value) != vals.end()) {
      occurrence_count += 1;
      if (date < now) {
        continue;
      }
      if (repeat.repeater() == 1 || occurrence_count % repeat.repeater() == 0) {
        dates.push_back(date);
      }
    }
  }

  return dates;
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

  for (const auto& type : transaction_types_) {
    if (lasts.from_history.find(type.category) != lasts.from_history.end()) {
      continue;
    }
    if (lasts.from_exception.find(type.category) !=
        lasts.from_exception.end()) {
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
    auto dates = build_date_list(trans_type.repetition, now, it->second, days);
    for (const auto& date : dates) {
      Date adjusted =
          repetition.auto_flag() ? adjust_to_business_day(date) : date;
      if (adjusted <= it->second) {
        continue;
      }
      double amount = trans_type.amount;
      auto key_adjusted =
          adjusted.to_mm_dd_yyyy() + ":" + trans_type.category;
      auto key_scheduled = date.to_mm_dd_yyyy() + ":" + trans_type.category;
      auto ex_it = exception_for.find(key_adjusted);
      if (ex_it != exception_for.end()) {
        amount = ex_it->second;
      } else {
        auto ex_sched = exception_for.find(key_scheduled);
        if (ex_sched != exception_for.end()) {
          amount = ex_sched->second;
        }
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
    if (exc.date < now) {
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
