#include "cache.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>

#include "paths.h"

namespace budget::io {

namespace {

constexpr char kMagic[4] = {'B', 'D', 'G', '1'};

void write_u32(std::ostream& out, uint32_t value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool read_u32(std::istream& in, uint32_t& value) {
  return static_cast<bool>(
      in.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

void write_i32(std::ostream& out, int32_t value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool read_i32(std::istream& in, int32_t& value) {
  return static_cast<bool>(
      in.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

void write_double(std::ostream& out, double value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool read_double(std::istream& in, double& value) {
  return static_cast<bool>(
      in.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

void write_string(std::ostream& out, const std::string& value) {
  write_u32(out, static_cast<uint32_t>(value.size()));
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool read_string(std::istream& in, std::string& value) {
  uint32_t size = 0;
  if (!read_u32(in, size)) {
    return false;
  }
  std::string buf(size, '\0');
  if (!in.read(buf.data(), static_cast<std::streamsize>(size))) {
    return false;
  }
  value = std::move(buf);
  return true;
}

bool read_magic(std::istream& in) {
  char magic[4];
  if (!in.read(magic, sizeof(magic))) {
    return false;
  }
  return std::memcmp(magic, kMagic, sizeof(magic)) == 0;
}

bool write_magic(std::ostream& out) {
  out.write(kMagic, sizeof(kMagic));
  return static_cast<bool>(out);
}

}  // namespace

Cache::Cache(std::string name, std::string profile)
    : name_(std::move(name)), profile_(std::move(profile)) {}

std::string Cache::cache_dir() const {
  std::string dir = budget::io::cache_dir();
  if (dir.empty()) {
    return {};
  }
  return dir + "/" + name_;
}

std::string Cache::cache_file(const std::string& key,
                              const std::string& profile_override) const {
  std::string profile = profile_override.empty() ? profile_ : profile_override;
  std::string filename = key + ".bin";
  if (key != "profiles" && !profile.empty() && profile != "Main") {
    filename = profile + "-" + filename;
  }
  std::string dir = cache_dir();
  if (dir.empty()) {
    return {};
  }
  std::filesystem::create_directories(dir);
  return dir + "/" + filename;
}

std::vector<budget::Profile> Cache::read_profiles(
    const std::vector<budget::Profile>& fallback) const {
  std::string path = cache_file("profiles", "Main");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::vector<budget::Profile> result;
  result.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    budget::Profile p;
    if (!read_string(in, p.name) || !read_string(in, p.description) ||
        !read_double(in, p.balance)) {
      return fallback;
    }
    result.push_back(std::move(p));
  }
  return result;
}

bool Cache::write_profiles(const std::vector<budget::Profile>& profiles) const {
  std::string path = cache_file("profiles", "Main");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(profiles.size()));
  for (const auto& p : profiles) {
    write_string(out, p.name);
    write_string(out, p.description);
    write_double(out, p.balance);
  }
  return static_cast<bool>(out);
}

bool Cache::delete_profile(const std::string& profile) const {
  if (profile == "Main") {
    return false;
  }
  std::string dir = cache_dir();
  if (dir.empty()) {
    return false;
  }
  std::filesystem::path base(dir);
  bool removed = false;
  for (const auto& entry : std::filesystem::directory_iterator(base)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto filename = entry.path().filename().string();
    if (filename.rfind(profile + "-", 0) == 0) {
      std::filesystem::remove(entry.path());
      removed = true;
    }
  }
  return removed;
}

bool Cache::copy_profile(const std::string& from_profile,
                         const std::string& to_profile) const {
  std::string dir = cache_dir();
  if (dir.empty() || !std::filesystem::exists(dir)) {
    return false;
  }
  if (from_profile == to_profile) {
    return false;
  }
  std::filesystem::path base(dir);
  std::regex main_regex(R"(^([A-Za-z0-9_]+)\.bin$)");
  std::regex prefixed_regex(R"(^([A-Za-z0-9_]+)-([A-Za-z0-9_]+)\.bin$)");
  bool copied = false;
  for (const auto& entry : std::filesystem::directory_iterator(base)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::string filename = entry.path().filename().string();
    std::smatch match;
    std::string key;
    if (from_profile == "Main") {
      if (!std::regex_match(filename, match, main_regex)) {
        continue;
      }
      key = match[1].str();
    } else {
      if (!std::regex_match(filename, match, prefixed_regex)) {
        continue;
      }
      if (match[1].str() != from_profile) {
        continue;
      }
      key = match[2].str();
    }
    if (key.empty()) {
      continue;
    }
    std::string to_filename =
        to_profile == "Main" ? key + ".bin" : to_profile + "-" + key + ".bin";
    std::filesystem::path to_path = base / to_filename;
    std::filesystem::copy_file(entry.path(), to_path,
                               std::filesystem::copy_options::overwrite_existing);
    copied = true;
  }
  return copied;
}

std::optional<std::string> Cache::read_session_profile() const {
  std::string path = cache_file("session", "Main");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return std::nullopt;
  }
  std::string profile;
  if (!read_string(in, profile)) {
    return std::nullopt;
  }
  return profile;
}

bool Cache::write_session_profile(const std::string& profile) const {
  std::string path = cache_file("session", "Main");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_string(out, profile);
  return static_cast<bool>(out);
}

std::vector<budget::TransactionType> Cache::read_transaction_types(
    const std::vector<budget::TransactionType>& fallback) const {
  std::string path = cache_file("transaction_types");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::vector<budget::TransactionType> result;
  result.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    budget::TransactionType t;
    if (!read_string(in, t.category) || !read_string(in, t.repetition) ||
        !read_double(in, t.amount) || !read_string(in, t.description_regex) ||
        !read_string(in, t.debit_regex)) {
      return fallback;
    }
    result.push_back(std::move(t));
  }
  return result;
}

bool Cache::write_transaction_types(
    const std::vector<budget::TransactionType>& types) const {
  std::string path = cache_file("transaction_types");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(types.size()));
  for (const auto& t : types) {
    write_string(out, t.category);
    write_string(out, t.repetition);
    write_double(out, t.amount);
    write_string(out, t.description_regex);
    write_string(out, t.debit_regex);
  }
  return static_cast<bool>(out);
}

bool Cache::delete_transaction_types(const std::string& profile) const {
  std::string path = cache_file("transaction_types", profile);
  if (path.empty()) {
    return false;
  }
  return std::filesystem::remove(path);
}

std::vector<budget::Exception> Cache::read_exceptions(
    const std::vector<budget::Exception>& fallback) const {
  std::string path = cache_file("exceptions");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::vector<budget::Exception> result;
  result.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    budget::Exception e;
    std::string date;
    if (!read_string(in, e.category) || !read_string(in, date) ||
        !read_double(in, e.amount)) {
      return fallback;
    }
    auto parsed = budget::Date::parse_mm_dd_yyyy(date);
    if (!parsed.has_value()) {
      return fallback;
    }
    e.date = *parsed;
    result.push_back(std::move(e));
  }
  return result;
}

bool Cache::write_exceptions(
    const std::vector<budget::Exception>& exceptions) const {
  std::string path = cache_file("exceptions");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(exceptions.size()));
  for (const auto& e : exceptions) {
    write_string(out, e.category);
    write_string(out, e.date.to_mm_dd_yyyy());
    write_double(out, e.amount);
  }
  return static_cast<bool>(out);
}

bool Cache::delete_exceptions(const std::string& profile) const {
  std::string path = cache_file("exceptions", profile);
  if (path.empty()) {
    return false;
  }
  return std::filesystem::remove(path);
}

std::unordered_map<std::string, std::string> Cache::read_lasts(
    const std::unordered_map<std::string, std::string>& fallback) const {
  std::string path = cache_file("lasts");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::unordered_map<std::string, std::string> result;
  for (uint32_t i = 0; i < count; ++i) {
    std::string key;
    std::string value;
    if (!read_string(in, key) || !read_string(in, value)) {
      return fallback;
    }
    result[key] = value;
  }
  return result;
}

bool Cache::write_lasts(
    const std::unordered_map<std::string, std::string>& lasts) const {
  std::string path = cache_file("lasts");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(lasts.size()));
  for (const auto& pair : lasts) {
    write_string(out, pair.first);
    write_string(out, pair.second);
  }
  return static_cast<bool>(out);
}

std::unordered_map<std::string, std::string> Cache::read_last_sources(
    const std::unordered_map<std::string, std::string>& fallback) const {
  std::string path = cache_file("last_sources");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::unordered_map<std::string, std::string> result;
  for (uint32_t i = 0; i < count; ++i) {
    std::string key;
    std::string value;
    if (!read_string(in, key) || !read_string(in, value)) {
      return fallback;
    }
    result[key] = value;
  }
  return result;
}

bool Cache::write_last_sources(
    const std::unordered_map<std::string, std::string>& sources) const {
  std::string path = cache_file("last_sources");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(sources.size()));
  for (const auto& pair : sources) {
    write_string(out, pair.first);
    write_string(out, pair.second);
  }
  return static_cast<bool>(out);
}

std::unordered_map<std::string, Cache::ThemeRecord> Cache::read_themes(
    const std::unordered_map<std::string, ThemeRecord>& fallback) const {
  std::string path = cache_file("themes");
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open() || !read_magic(in)) {
    return fallback;
  }
  uint32_t count = 0;
  if (!read_u32(in, count)) {
    return fallback;
  }
  std::unordered_map<std::string, ThemeRecord> result;
  for (uint32_t i = 0; i < count; ++i) {
    std::string key;
    ThemeRecord rec;
    if (!read_string(in, key)) {
      return fallback;
    }
    int32_t fg = 0;
    int32_t bg = 0;
    if (!read_i32(in, fg) || !read_i32(in, bg)) {
      return fallback;
    }
    rec.fg = static_cast<int>(fg);
    rec.bg = static_cast<int>(bg);
    if (!read_string(in, rec.style)) {
      return fallback;
    }
    result[key] = rec;
  }
  return result;
}

bool Cache::write_themes(
    const std::unordered_map<std::string, ThemeRecord>& themes) const {
  std::string path = cache_file("themes");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!write_magic(out)) {
    return false;
  }
  write_u32(out, static_cast<uint32_t>(themes.size()));
  for (const auto& pair : themes) {
    write_string(out, pair.first);
    write_i32(out, static_cast<int32_t>(pair.second.fg));
    write_i32(out, static_cast<int32_t>(pair.second.bg));
    write_string(out, pair.second.style);
  }
  return static_cast<bool>(out);
}

}  // namespace budget::io
