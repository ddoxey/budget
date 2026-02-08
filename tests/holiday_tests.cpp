#include <gtest/gtest.h>

#include "../src/core/holidays.h"
#include "../src/core/budget.h"

using budget::Date;
using budget::adjust_to_business_day;
using budget::is_federal_holiday;
using budget::Budget;
using budget::TransactionType;
using budget::Exception;

TEST(BusinessDayAdjust, ShiftsWeekendsToFriday) {
  Date sat{2026, 2, 7};
  Date sun{2026, 2, 8};
  EXPECT_EQ(adjust_to_business_day(sat).to_mm_dd_yyyy(), "02-06-2026");
  EXPECT_EQ(adjust_to_business_day(sun).to_mm_dd_yyyy(), "02-06-2026");
}

TEST(BusinessDayAdjust, ShiftsFederalHolidaysToPriorBusinessDay) {
  Date july4{2025, 7, 4};  // Friday holiday
  EXPECT_TRUE(is_federal_holiday(july4));
  EXPECT_EQ(adjust_to_business_day(july4).to_mm_dd_yyyy(), "07-03-2025");

  Date christmas_observed{2021, 12, 24};  // observed for 12/25/2021
  EXPECT_TRUE(is_federal_holiday(christmas_observed));
  EXPECT_EQ(adjust_to_business_day(christmas_observed).to_mm_dd_yyyy(),
            "12-23-2021");
}

TEST(BusinessDayAdjust, IncludesObservedNewYearsFromNextYear) {
  Date observed_new_years{2021, 12, 31};  // observed for 01/01/2022
  EXPECT_TRUE(is_federal_holiday(observed_new_years));
  EXPECT_EQ(adjust_to_business_day(observed_new_years).to_mm_dd_yyyy(),
            "12-30-2021");
}

TEST(BusinessDayAdjust, LeavesBusinessDayUnchanged) {
  Date thursday{2026, 2, 5};
  EXPECT_EQ(adjust_to_business_day(thursday).to_mm_dd_yyyy(), "02-05-2026");
}

TEST(RepetitionAutoFlag, NonAutoDoesNotShiftWeekend) {
  TransactionType rent{"Rent", "Sat", -100.0, "", ""};
  Date now{2026, 2, 1};
  Budget budget(0.0, {rent}, {}, {}, 14, 0, now);
  auto events = budget.events();
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events[0].get_date().to_mm_dd_yyyy(), "02-07-2026");
}

TEST(RepetitionAutoFlag, AutoShiftsWeekendToFriday) {
  TransactionType rent{"Rent", "@Sat", -100.0, "", ""};
  Date now{2026, 2, 1};
  Budget budget(0.0, {rent}, {}, {}, 14, 0, now);
  auto events = budget.events();
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events[0].get_date().to_mm_dd_yyyy(), "02-06-2026");
}

TEST(RepetitionAutoFlag, AutoShiftsHolidayToPriorBusinessDay) {
  TransactionType pay{"Pay", "@Fri", 100.0, "", ""};
  Exception exc{"Pay", Date{2025, 7, 4}, 100.0};
  Date now{2025, 7, 1};
  Budget budget(0.0, {pay}, {exc}, {}, 10, 0, now);
  auto events = budget.events();
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events[0].get_date().to_mm_dd_yyyy(), "07-03-2025");
}
