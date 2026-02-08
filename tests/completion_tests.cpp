#include <iostream>
#include <vector>

#include "../src/cli/completion.h"

static int failures = 0;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    failures++;
  }
}

bool contains(const std::vector<std::string>& items, const std::string& value) {
  for (const auto& item : items) {
    if (item == value) {
      return true;
    }
  }
  return false;
}

int main() {
  {
    auto matches = budget::cli::completion_candidates("upd", 3);
    expect(contains(matches, "update"), "Complete first token prefix");
  }
  {
    auto matches = budget::cli::completion_candidates("update ", 7);
    expect(contains(matches, "exception"), "Suggest update subcommands");
    expect(contains(matches, "header"), "Suggest update header");
  }
  {
    auto matches = budget::cli::completion_candidates("update exc", 10);
    expect(contains(matches, "exception"), "Complete update exception token");
  }
  {
    auto matches = budget::cli::completion_candidates("themes sho", 10);
    expect(contains(matches, "show"), "Complete themes show token");
  }
  {
    auto matches = budget::cli::completion_candidates("themes show d", 13);
    expect(contains(matches, "default"), "Complete themes show default token");
  }

  if (failures > 0) {
    std::cerr << failures << " test(s) failed.\n";
    return 1;
  }
  std::cout << "All completion tests passed.\n";
  return 0;
}
