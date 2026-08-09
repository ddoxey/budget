#pragma once

#include <string>
#include <unordered_map>

namespace budget::cli {

struct HelpTopic {
  std::string summary;
  std::string details;
};

const std::unordered_map<std::string, HelpTopic>& help_topics();
const std::string& repetition_syntax_help();

}  // namespace budget::cli
