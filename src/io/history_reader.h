#pragma once

#include <string>
#include <vector>

#include "../core/budget.h"
#include "header_map.h"

namespace budget::io {

struct HistoryData {
  std::vector<budget::Transaction> transactions;
  HeaderMap header_map;
};

HistoryData read_transaction_history(const std::string& path,
                                     const std::string& header_map_path);
HistoryData read_transaction_histories(const std::vector<std::string>& paths,
                                       const std::string& header_map_path);

}  // namespace budget::io
