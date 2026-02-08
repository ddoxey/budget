#pragma once

#include "date.h"

namespace budget {

bool is_federal_holiday(const Date& date);
bool is_business_day(const Date& date);
Date adjust_to_business_day(const Date& date);

}  // namespace budget
