#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/budget.h"

namespace budget::io {

class Cache {
 public:
  Cache(std::string name, std::string profile);

  std::string cache_dir() const;
  std::string cache_file(const std::string& key,
                         const std::string& profile_override = "") const;

  std::vector<budget::Profile> read_profiles(
      const std::vector<budget::Profile>& fallback = {}) const;
  bool write_profiles(const std::vector<budget::Profile>& profiles) const;
  bool delete_profile(const std::string& profile) const;
  bool copy_profile(const std::string& from_profile,
                    const std::string& to_profile) const;
  std::optional<std::string> read_session_profile() const;
  bool write_session_profile(const std::string& profile) const;

  std::vector<budget::TransactionType> read_transaction_types(
      const std::vector<budget::TransactionType>& fallback = {}) const;
  bool write_transaction_types(
      const std::vector<budget::TransactionType>& types) const;
  bool delete_transaction_types(const std::string& profile) const;

  std::vector<budget::Exception> read_exceptions(
      const std::vector<budget::Exception>& fallback = {}) const;
  bool write_exceptions(const std::vector<budget::Exception>& exceptions) const;
  bool delete_exceptions(const std::string& profile) const;

  std::unordered_map<std::string, std::string> read_lasts(
      const std::unordered_map<std::string, std::string>& fallback = {}) const;
  bool write_lasts(
      const std::unordered_map<std::string, std::string>& lasts) const;

  struct ThemeRecord {
    int fg = -1;
    int bg = -1;
    std::string style;
  };

  std::unordered_map<std::string, ThemeRecord> read_themes(
      const std::unordered_map<std::string, ThemeRecord>& fallback = {}) const;
  bool write_themes(
      const std::unordered_map<std::string, ThemeRecord>& themes) const;

 private:
  std::string name_;
  std::string profile_;
};

}  // namespace budget::io
