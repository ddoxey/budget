#include "completion.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "help.h"

namespace budget::cli {

namespace {

std::vector<std::string> command_list() {
  return {
      "help",     "headers", "clear", "update", "del",          "copy",
      "profile",  "profiles","balance","run",   "trans",        "transactions",
      "exceptions","themes", "status","save",   "reload",       "totals",
      "cats",     "lasts",
      "themeconfig","exit",  "quit",  "q",      "x"};
}

std::vector<std::string> update_subcommands() {
  return {"header", "profile", "transaction", "exception", "theme", "last"};
}

std::vector<std::string> del_subcommands() {
  return {"header", "profile", "exception", "transaction", "theme", "last"};
}

std::vector<std::string> themes_subcommands() {
  return {"randomize", "rotate", "reset", "show"};
}

std::vector<std::string> copy_subcommands() {
  return {"profile"};
}

std::vector<std::string> help_subcommands() {
  std::vector<std::string> keys;
  keys.reserve(help_topics().size());
  for (const auto& pair : help_topics()) {
    keys.push_back(pair.first);
  }
  return keys;
}

std::vector<std::string> internal_header_keys() {
  return {"transaction_date", "description", "debit"};
}

std::function<std::vector<std::string>()> g_category_provider;
std::function<std::vector<std::string>()> g_profile_provider;

std::vector<std::string> filter_prefix(const std::vector<std::string>& options,
                                       const std::string& prefix) {
  std::vector<std::string> out;
  for (const auto& opt : options) {
    if (opt.rfind(prefix, 0) == 0) {
      out.push_back(opt);
    }
  }
  return out;
}

}  // namespace

void set_category_provider(std::function<std::vector<std::string>()> provider) {
  g_category_provider = std::move(provider);
}

void set_profile_provider(std::function<std::vector<std::string>()> provider) {
  g_profile_provider = std::move(provider);
}

std::vector<std::string> completion_candidates(const std::string& buffer,
                                               size_t cursor) {
  std::string head = buffer.substr(0, cursor);
  std::istringstream iss(head);
  std::vector<std::string> tokens;
  std::string tok;
  while (iss >> tok) {
    tokens.push_back(tok);
  }

  bool at_token_start =
      head.empty() || std::isspace(static_cast<unsigned char>(head.back()));
  std::string prefix;
  if (!at_token_start && !tokens.empty()) {
    prefix = tokens.back();
  }

  if (tokens.empty()) {
    return filter_prefix(command_list(), prefix);
  }
  if (tokens.size() == 1 && !at_token_start) {
    return filter_prefix(command_list(), prefix);
  }

  const std::string& cmd = tokens[0];
  if (cmd == "update") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      return filter_prefix(update_subcommands(), prefix);
    }
    if (tokens.size() == 2 && at_token_start) {
      return update_subcommands();
    }
    if (tokens.size() == 3 && tokens[1] == "header") {
      return filter_prefix(internal_header_keys(), prefix);
    }
    if (tokens.size() == 3 && tokens[1] == "transaction") {
      if (g_category_provider) {
        return filter_prefix(g_category_provider(), prefix);
      }
    }
    if (tokens.size() == 3 && tokens[1] == "profile") {
      if (g_profile_provider) {
        return filter_prefix(g_profile_provider(), prefix);
      }
    }
    if (tokens.size() == 3 && tokens[1] == "theme") {
      if (g_category_provider) {
        auto cats = g_category_provider();
        cats.push_back("default");
        return filter_prefix(cats, prefix);
      }
    }
    if (tokens.size() == 3 && tokens[1] == "exception") {
      if (g_category_provider) {
        return filter_prefix(g_category_provider(), prefix);
      }
    }
    if (tokens.size() == 3 && tokens[1] == "last") {
      if (g_category_provider) {
        return filter_prefix(g_category_provider(), prefix);
      }
    }
  }
  if (cmd == "del") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      return filter_prefix(del_subcommands(), prefix);
    }
    if (tokens.size() == 2 && at_token_start) {
      return del_subcommands();
    }
    if (tokens.size() == 3 && tokens[1] == "header") {
      return filter_prefix(internal_header_keys(), prefix);
    }
    if (tokens.size() == 3 &&
        (tokens[1] == "transaction" || tokens[1] == "exception" ||
         tokens[1] == "last" || tokens[1] == "theme")) {
      if (g_category_provider) {
        return filter_prefix(g_category_provider(), prefix);
      }
    }
  }
  if (cmd == "themes") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      return filter_prefix(themes_subcommands(), prefix);
    }
    if (tokens.size() == 2 && at_token_start) {
      return themes_subcommands();
    }
    if (tokens.size() == 3 && tokens[1] == "show") {
      return filter_prefix(std::vector<std::string>{"default"}, prefix);
    }
  }
  if (cmd == "copy") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      return filter_prefix(copy_subcommands(), prefix);
    }
    if (tokens.size() == 2 && at_token_start) {
      return copy_subcommands();
    }
  }
  if (cmd == "help") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      return filter_prefix(help_subcommands(), prefix);
    }
    if (tokens.size() == 2 && at_token_start) {
      return help_subcommands();
    }
  }
  if (cmd == "profile") {
    if (tokens.size() == 1 || (tokens.size() == 2 && !at_token_start)) {
      if (g_profile_provider) {
        return filter_prefix(g_profile_provider(), prefix);
      }
    }
    if (tokens.size() == 2 && at_token_start) {
      if (g_profile_provider) {
        return g_profile_provider();
      }
    }
  }
  if (cmd == "copy" && tokens.size() >= 2 && tokens[1] == "profile") {
    if (tokens.size() == 3 || (tokens.size() == 4 && !at_token_start)) {
      if (g_profile_provider) {
        return filter_prefix(g_profile_provider(), prefix);
      }
    }
    if (tokens.size() == 4 && at_token_start) {
      if (g_profile_provider) {
        return g_profile_provider();
      }
    }
  }
  return filter_prefix(command_list(), prefix);
}

}  // namespace budget::cli
