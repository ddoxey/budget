#include <iostream>
#include <vector>

#include "../src/core/budget.h"
#include "../src/core/date.h"
#include "../src/core/money.h"
#include "../src/core/repetition.h"
#include "../src/io/history_reader.h"

using budget::Budget;
using budget::Date;
using budget::Exception;
using budget::Money;
using budget::Repetition;
using budget::Transaction;
using budget::TransactionType;
using budget::io::read_transaction_history;

static int failures = 0;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    failures++;
  }
}

int main() {
  {
    auto parsed = Money::parse("$1,234.50");
    expect(parsed.has_value() && *parsed == 1234.50, "Money parse with commas");
    Money m(1234.5, "$");
    expect(m.str() == "$1,234.50", "Money format with symbol");
  }

  {
    Repetition rep("Tue/2");
    expect(rep.repeater() == 2, "Repetition repeater");
    expect(rep.field() == "%a", "Repetition field weekday");
    expect(rep.values().size() == 1 && rep.values()[0] == "Tue",
           "Repetition weekday values");
  }

  {
    TransactionType payday{"Payday", "Fri", 100.0, "", ""};
    Date now{2026, 2, 7};  // Saturday
    Budget budget(0.0, {payday}, {}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 2, "Budget event count for Fri over 14 days");
    if (events.size() == 2) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "First Friday event date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-20-2026",
             "Second Friday event date");
    }
  }

  {
    TransactionType payday{"Payday", "Fri", 100.0, "", ""};
    Date now{2026, 2, 7};
    Exception exc{"Payday", Date{2026, 2, 13}, 250.0};
    Budget budget(0.0, {payday}, {exc}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 2, "Budget event count with exception");
    if (events.size() == 2) {
      expect(events[0].get_amount() == 250.0,
             "Exception amount applied on date");
    }
  }

  {
    Date now{2026, 2, 7};
    std::vector<Exception> exceptions = {
        {"Rent", Date{2026, 1, 1}, -1000.0},
        {"Rent", Date{2026, 2, 1}, -1100.0},
        {"Rent", Date{2026, 3, 1}, -1200.0},
        {"Gym", Date{2025, 12, 1}, -40.0},
        {"Gym", Date{2026, 1, 1}, -45.0},
        {"Gym", Date{2026, 12, 1}, -50.0},
    };
    auto filtered = budget::filter_exceptions(exceptions, now);
    int rent_past = 0;
    int gym_past = 0;
    int future = 0;
    for (const auto& exc : filtered) {
      if (exc.date < now) {
        if (exc.category == "Rent") {
          rent_past += 1;
          expect(exc.amount == -1100.0, "Latest past Rent exception preserved");
        }
        if (exc.category == "Gym") {
          gym_past += 1;
          expect(exc.amount == -45.0, "Latest past Gym exception preserved");
        }
      } else {
        future += 1;
      }
    }
    expect(rent_past == 1, "Single past Rent exception preserved");
    expect(gym_past == 1, "Single past Gym exception preserved");
    expect(future == 2, "All future exceptions preserved");
  }

  {
    auto history = read_transaction_history("var/tranactions.csv",
                                            "/tmp/budget_header_map.json");
    expect(!history.transactions.empty(), "CSV history loaded");
    if (!history.transactions.empty()) {
      const auto& fields = history.transactions[0].fields;
      expect(fields.find("transaction_date") != fields.end(),
             "Header mapping includes transaction_date");
      expect(fields.find("description") != fields.end(),
             "Header mapping includes description");
      expect(fields.find("debit") != fields.end(),
             "Header mapping includes debit");
    }
  }

  if (failures > 0) {
    std::cerr << failures << " test(s) failed.\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
