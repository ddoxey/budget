#include "dotchart.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace budget::ui {

namespace {

std::string find_dotchart() {
  const char* path = std::getenv("PATH");
  if (!path) {
    return "/usr/local/bin/dotchart";
  }
  return "dotchart";
}

}  // namespace

std::optional<std::string> render_dotchart(
    const std::vector<DotchartPoint>& points, int width) {
  if (points.empty()) {
    return std::nullopt;
  }

  std::ostringstream input;
  for (const auto& point : points) {
    input.setf(std::ios::fixed);
    input.precision(2);
    input << point.forecast_day << "," << point.balance << "\n";
  }

  std::filesystem::path temp_path =
      std::filesystem::temp_directory_path() / "budget_dotchart_input.txt";
  {
    std::ofstream out(temp_path);
    if (!out.is_open()) {
      return std::nullopt;
    }
    out << input.str();
  }

  std::ostringstream cmd;
  cmd << find_dotchart() << " -F, -c 2 -y %0.0f --x-min-axis --color";
  if (width > 0) {
    cmd << " --width " << width;
  }
  cmd << " < " << temp_path.string();

  FILE* pipe = popen(cmd.str().c_str(), "r");
  if (!pipe) {
    std::filesystem::remove(temp_path);
    return std::nullopt;
  }

  std::string output;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
  int rc = pclose(pipe);
  std::filesystem::remove(temp_path);
  if (rc != 0) {
    return std::nullopt;
  }

  return output;
}

}  // namespace budget::ui
