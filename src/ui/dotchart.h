#pragma once

#include <optional>
#include <string>
#include <vector>

namespace budget::ui {

std::optional<std::string> render_dotchart(const std::vector<double>& balances,
                                           int width = 0);

}  // namespace budget::ui
