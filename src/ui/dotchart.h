#pragma once

#include <optional>
#include <string>
#include <vector>

namespace budget::ui {

struct DotchartPoint {
  int forecast_day = 0;
  double balance = 0.0;
};

std::optional<std::string> render_dotchart(const std::vector<DotchartPoint>& points,
                                           int width = 0);

}  // namespace budget::ui
