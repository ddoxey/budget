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
    Repetition rep("1,15/2");
    expect(rep.repeater() == 2, "Monthly repetition repeater");
    expect(rep.field() == "%d", "Monthly repetition field day-of-month");
    expect(rep.values().size() == 2 && rep.values()[0] == "01" &&
               rep.values()[1] == "15",
           "Monthly repetition values");
    expect(rep.monthly_factor() == 1.0,
           "Monthly factor accounts for multiple days and repeater");
  }

  {
    Repetition rep("2xWeek");
    auto positions = rep.positions_in_week();
    expect(rep.kind() == budget::RepetitionKind::CountPerWeek,
           "Count weekly repetition kind");
    expect(positions.size() == 2 && positions[0] == 0 && positions[1] == 4,
           "Count weekly positions default to Sun/Thu");
    expect(rep.monthly_factor() == 8.0,
           "Count weekly monthly factor scales by count");
    expect(rep.to_string() == "Twice a week",
           "Count weekly display text");
  }

  {
    Repetition rep("2xWeek+2");
    auto positions = rep.positions_in_week();
    expect(positions.size() == 2 && positions[0] == 2 && positions[1] == 6,
           "Count weekly offset rotates positions");
    expect(rep.to_string() == "Twice a week (starting on Tue)",
           "Count weekly offset display text");
  }

  {
    Repetition rep("2xMonth");
    auto positions = rep.positions_in_month(30);
    expect(rep.kind() == budget::RepetitionKind::CountPerMonth,
           "Count monthly repetition kind");
    expect(positions.size() == 2 && positions[0] == 1 && positions[1] == 15,
           "Count monthly positions anchor from first day");
    expect(rep.monthly_factor() == 2.0,
           "Count monthly monthly factor matches count");
  }

  {
    Repetition rep("2xMonth+2");
    auto positions = rep.positions_in_month(30);
    expect(positions.size() == 2 && positions[0] == 3 && positions[1] == 17,
           "Count monthly offset shifts positions");
    expect(rep.to_string() == "Twice a month (starting on 3rd)",
           "Count monthly offset display text");
  }

  {
    Repetition rep("2xMonth+1");
    expect(rep.to_string() == "Twice a month (starting on 2nd)",
           "Count monthly offset display matches requested wording");
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
    TransactionType semimonthly{"Semimonthly", "1,15", 100.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "Semimonthly";
    history_tx.fields["transaction_date"] = "02/01/2026";
    Date now{2026, 2, 7};
    Budget budget(0.0, {semimonthly}, {}, {history_tx}, 40, 0, now);
    auto events = budget.events();
    expect(events.size() == 3, "Semimonthly event count");
    if (events.size() == 3) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-15-2026",
             "Semimonthly second February date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "03-01-2026",
             "Semimonthly first March date");
      expect(events[2].get_date().to_mm_dd_yyyy() == "03-15-2026",
             "Semimonthly second March date");
    }
  }

  {
    TransactionType semimonthly{"BimonthlySemi", "1,15/2", 100.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "BimonthlySemi";
    history_tx.fields["transaction_date"] = "01/01/2026";
    Date now{2026, 1, 2};
    Budget budget(0.0, {semimonthly}, {}, {history_tx}, 80, 0, now);
    auto events = budget.events();
    expect(events.size() == 3, "Bimonthly semimonthly event count");
    if (events.size() == 3) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "01-15-2026",
             "Bimonthly semimonthly keeps same-month second date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "03-01-2026",
             "Bimonthly semimonthly skips February first date");
      expect(events[2].get_date().to_mm_dd_yyyy() == "03-15-2026",
             "Bimonthly semimonthly skips February second date");
    }
  }

  {
    TransactionType counted{"CountWeek", "2xWeek", 25.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "CountWeek";
    history_tx.fields["transaction_date"] = "02/05/2026";
    Date now{2026, 2, 7};
    Budget budget(0.0, {counted}, {}, {history_tx}, 10, 0, now);
    auto events = budget.events();
    expect(events.size() == 3, "Count weekly event count");
    if (events.size() == 3) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-08-2026",
             "Count weekly first date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-12-2026",
             "Count weekly second date");
      expect(events[2].get_date().to_mm_dd_yyyy() == "02-15-2026",
             "Count weekly third date");
    }
  }

  {
    TransactionType counted{"CountWeekOffset", "2xWeek+2", 25.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "CountWeekOffset";
    history_tx.fields["transaction_date"] = "02/07/2026";
    Date now{2026, 2, 8};
    Budget budget(0.0, {counted}, {}, {history_tx}, 7, 0, now);
    auto events = budget.events();
    expect(events.size() == 2, "Count weekly offset event count");
    if (events.size() == 2) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-10-2026",
             "Count weekly offset first date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-14-2026",
             "Count weekly offset second date");
    }
  }

  {
    TransactionType counted{"CountMonth", "2xMonth+2", 50.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "CountMonth";
    history_tx.fields["transaction_date"] = "04/03/2026";
    Date now{2026, 4, 4};
    Budget budget(0.0, {counted}, {}, {history_tx}, 50, 0, now);
    auto events = budget.events();
    expect(events.size() == 3, "Count monthly offset event count");
    if (events.size() == 3) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "04-17-2026",
             "Count monthly offset second April date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "05-03-2026",
             "Count monthly offset first May date");
      expect(events[2].get_date().to_mm_dd_yyyy() == "05-18-2026",
             "Count monthly offset second May date");
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
    TransactionType income{"Income", "Fri", 500.0, "", ""};
    TransactionType bill{"Bill", "Fri", -200.0, "", ""};
    Date now{2026, 2, 7};
    Budget budget(0.0, {income, bill}, {}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 4, "Budget event count for same-date sorting");
    if (events.size() == 4) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "First same-date event date");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Second same-date event date");
      expect(events[0].get_amount() == 500.0,
             "Same-date events sorted by amount desc (first)");
      expect(events[1].get_amount() == -200.0,
             "Same-date events sorted by amount desc (second)");
    }
  }

  {
    TransactionType alpha{"Alpha", "Fri", 100.0, "", ""};
    TransactionType beta{"Beta", "Fri", 100.0, "", ""};
    TransactionType gamma{"Gamma", "Fri", -50.0, "", ""};
    Date now{2026, 2, 7};
    Budget budget(0.0, {alpha, beta, gamma}, {}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 6, "Budget event count for tie sorting");
    if (events.size() == 6) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Tie sorting same-date event date (first)");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Tie sorting same-date event date (second)");
      expect(events[2].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Tie sorting same-date event date (third)");
      expect(events[0].get_amount() == 100.0,
             "Same-date tie amount desc (first)");
      expect(events[1].get_amount() == 100.0,
             "Same-date tie amount desc (second)");
      expect(events[2].get_amount() == -50.0,
             "Same-date tie amount desc (third)");
    }
  }

  {
    TransactionType small{"Small", "Fri", -100.0, "", ""};
    TransactionType large{"Large", "Fri", -300.0, "", ""};
    Date now{2026, 2, 7};
    Budget budget(0.0, {small, large}, {}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 4, "Budget event count for negative sorting");
    if (events.size() == 4) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Negative sorting same-date event date (first)");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-13-2026",
             "Negative sorting same-date event date (second)");
      expect(events[0].get_amount() == -100.0,
             "Same-date negative amount desc (first)");
      expect(events[1].get_amount() == -300.0,
             "Same-date negative amount desc (second)");
    }
  }

  {
    TransactionType zero_base{"ZeroBase", "Fri", 0.0, "", ""};
    TransactionType zero_exc{"ZeroException", "Fri", 50.0, "", ""};
    Date now{2026, 2, 7};
    Exception exc{"ZeroException", Date{2026, 2, 13}, 0.0};
    Budget budget(0.0, {zero_base, zero_exc}, {exc}, {}, 14, 0, now);
    auto events = budget.events();
    expect(events.size() == 1, "Only non-zero events are kept");
    if (events.size() == 1) {
      expect(events[0].get_amount() == 50.0,
             "Non-zero occurrence remains after zero exception");
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-20-2026",
             "Later non-zero date remains");
    }
  }

  {
    Date now{2026, 2, 7};
    std::vector<Exception> exceptions = {
        {"Rent", Date{2026, 1, 1}, -1000.0},
        {"Rent", Date{2026, 2, 1}, -1100.0},
        {"Rent", Date{2026, 2, 7}, -1150.0},
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
      if (exc.date <= now) {
        if (exc.category == "Rent") {
          rent_past += 1;
          expect(exc.amount == -1150.0,
                 "Latest through-today Rent exception preserved");
        }
        if (exc.category == "Gym") {
          gym_past += 1;
          expect(exc.amount == -45.0,
                 "Latest through-today Gym exception preserved");
        }
      } else {
        future += 1;
      }
    }
    expect(rent_past == 1, "Single through-today Rent exception preserved");
    expect(gym_past == 1, "Single through-today Gym exception preserved");
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

  {
    TransactionType paycheck{"Paycheck", "Fri", 100.0, "", ""};
    Transaction history_tx;
    history_tx.fields["cat"] = "Paycheck";
    history_tx.fields["transaction_date"] = "01/30/2026";
    Date now{2026, 2, 14};  // Saturday, one day after a missed Friday
    Budget budget(0.0, {paycheck}, {}, {history_tx}, 7, 0, now);
    auto lasts = budget.last_occurrence_details();
    auto events = budget.events();
    expect(lasts.find("Paycheck") != lasts.end(),
           "Last occurrence detail exists for overdue history");
    if (lasts.find("Paycheck") != lasts.end()) {
      expect(lasts["Paycheck"].date == "01-30-2026",
             "Overdue history keeps the actual last occurrence date");
      expect(lasts["Paycheck"].source == "history-overdue",
             "Overdue history source is labeled");
    }
    expect(events.size() == 2,
           "Overdue history produces catch-up today plus next future event");
    if (events.size() == 2) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-14-2026",
             "Catch-up event is inserted on today");
      expect(events[0].get_amount() == 200.0,
             "Catch-up event sums missed occurrences through today");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-20-2026",
             "Next future occurrence remains scheduled");
      expect(events[1].get_amount() == 100.0,
             "Future occurrence keeps base amount");
    }
  }

  {
    TransactionType groceries{"Groceries", "Fri", -75.0, "", ""};
    Date now{2026, 2, 14};  // Saturday, after a computed Friday occurrence
    Budget budget(0.0, {groceries}, {}, {}, 7, 0, now);
    auto lasts = budget.last_occurrence_details();
    auto events = budget.events();
    expect(lasts.find("Groceries") != lasts.end(),
           "Computed overdue detail exists");
    if (lasts.find("Groceries") != lasts.end()) {
      expect(lasts["Groceries"].date == "02-13-2026",
             "Computed overdue keeps the inferred last occurrence date");
      expect(lasts["Groceries"].source == "computed-overdue",
             "Computed overdue source is labeled internally");
    }
    expect(events.size() == 2,
           "Computed overdue produces catch-up today plus next future event");
    if (events.size() == 2) {
      expect(events[0].get_date().to_mm_dd_yyyy() == "02-14-2026",
             "Computed overdue catch-up is inserted on today");
      expect(events[0].get_amount() == -75.0,
             "Computed overdue catch-up includes the inferred missed amount");
      expect(events[1].get_date().to_mm_dd_yyyy() == "02-20-2026",
             "Computed overdue keeps the next future occurrence");
      expect(events[1].get_amount() == -75.0,
             "Future computed-overdue occurrence keeps base amount");
    }
  }

  {
    TransactionType groceries{"Groceries", "Fri", -75.0, "", ""};
    Date now{2026, 2, 14};
    Exception exc{"Groceries", Date{2026, 2, 14}, -120.0};
    Budget budget(0.0, {groceries}, {exc}, {}, 7, 0, now);
    auto lasts = budget.last_occurrence_details();
    expect(lasts.find("Groceries") != lasts.end(),
           "Exception-backed detail exists for computed category");
    if (lasts.find("Groceries") != lasts.end()) {
      expect(lasts["Groceries"].date == "02-14-2026",
             "Today exception becomes the last occurrence date");
      expect(lasts["Groceries"].source == "exception",
             "Today exception overrides computed source");
    }
  }

  {
    TransactionType groceries{"Groceries", "Fri", -75.0, "", ""};
    Date now{2026, 2, 14};
    Exception exc{"Groceries", Date{2026, 2, 13}, -120.0};
    Budget budget(0.0, {groceries}, {exc}, {}, 7, 0, now);
    auto lasts = budget.last_occurrence_details();
    expect(lasts.find("Groceries") != lasts.end(),
           "Equal-date exception detail exists");
    if (lasts.find("Groceries") != lasts.end()) {
      expect(lasts["Groceries"].date == "02-13-2026",
             "Equal-date exception keeps the shared last occurrence date");
      expect(lasts["Groceries"].source == "exception",
             "Equal-date exception overrides computed source");
    }
  }

  if (failures > 0) {
    std::cerr << failures << " test(s) failed.\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
