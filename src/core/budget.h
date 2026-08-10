#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "date.h"
#include "repetition.h"

namespace budget {

struct TransactionType {
  std::string category;
  std::string repetition;
  double amount = 0.0;
  std::string description_regex;
  std::string debit_regex;
};

struct Exception {
  std::string category;
  Date date;  // MM-DD-YYYY in EST
  double amount = 0.0;
};

struct Profile {
  std::string name;
  std::string description;
  double balance = 0.0;
};

struct Transaction {
  std::unordered_map<std::string, std::string> fields;
};

struct EventRecord {
  std::string category;
  double amount = 0.0;
  Date date;
  std::string yyyymmdd;
  int64_t epoch = 0;
};

struct LastOccurrence {
  std::string date;
  std::string source;
};

class Event {
 public:
  Event(const EventRecord& record, double& balance_ref);

  double get_amount() const { return amount_; }
  double get_balance() const { return balance_; }
  const std::string& get_category() const { return category_; }
  const Date& get_date() const { return date_; }
  int64_t get_epoch() const { return epoch_; }
  const std::string& get_yyyymmdd() const { return yyyymmdd_; }

 private:
  std::string category_;
  double amount_ = 0.0;
  double balance_ = 0.0;
  Date date_;
  std::string yyyymmdd_;
  int64_t epoch_ = 0;
};

class Chokepoint {
 public:
  Chokepoint(const Event& event);

  double balance() const { return balance_; }
  const Date& date() const { return date_; }
  int64_t timestamp() const { return timestamp_; }

 private:
  Date date_;
  double balance_ = 0.0;
  int64_t timestamp_ = 0;
};

class ChokepointList {
 public:
  ChokepointList(const std::vector<TransactionType>& transaction_types,
                 const std::vector<Event>& events);

  const std::vector<Chokepoint>& chokepoints() const { return chokepoints_; }
  const std::optional<Chokepoint>& minimum() const { return minimum_; }

  std::optional<Date> crash_date() const;

 private:
  std::vector<Chokepoint> chokepoints_;
  std::optional<Chokepoint> minimum_;
  std::vector<std::pair<double, double>> datapoints_;  // x=timestamp, y=balance
};

class Budget {
 public:
  Budget(double balance, std::vector<TransactionType> transaction_types,
         std::vector<Exception> exceptions, std::vector<Transaction> history,
         int days, int days_offset = 0,
         std::optional<Date> now_override = std::nullopt);

  double balance() const { return balance_; }
  int days() const { return days_; }
  const std::vector<Event>& events() const { return events_; }

  std::unordered_map<std::string, double> totals();
  ChokepointList chokepoints();

  std::unordered_map<std::string, std::string> last_occurrences() const;
  std::unordered_map<std::string, std::string> last_occurrence_sources() const;
  std::unordered_map<std::string, LastOccurrence> last_occurrence_details()
      const;
  std::vector<Event> events_by_category(const std::string& category) const;
  std::vector<Event> events_by_date(const std::string& date_mm_dd_yyyy) const;
  std::vector<Event> events_by_amount(double amount) const;

 private:
  std::vector<TransactionType> transaction_types_;
  std::vector<Event> events_;
  std::unordered_map<std::string, double> totals_;
  std::optional<ChokepointList> chokepoints_;
  std::unordered_map<std::string, Date> last_occurrence_of_;
  std::unordered_map<std::string, Date> schedule_anchor_of_;
  std::unordered_map<std::string, std::string> last_source_of_;
  int days_ = 0;
  double balance_ = 0.0;
};

std::vector<Exception> filter_exceptions(
    const std::vector<Exception>& exceptions, const Date& now);

}  // namespace budget
