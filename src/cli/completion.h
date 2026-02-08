#pragma once

#include <functional>
#include <string>
#include <vector>

namespace budget::cli {

std::vector<std::string> completion_candidates(const std::string& buffer,
                                               size_t cursor);
void set_category_provider(std::function<std::vector<std::string>()> provider);
void set_profile_provider(std::function<std::vector<std::string>()> provider);

}  // namespace budget::cli
