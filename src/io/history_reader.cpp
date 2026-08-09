#include "history_reader.h"

#include <iterator>

#include "csv_reader.h"

namespace budget::io {

HistoryData read_transaction_history(const std::string& path,
                                     const std::string& header_map_path) {
  auto doc = read_csv(path);
  auto map = load_or_guess_header_map(doc.headers, header_map_path);

  HistoryData data;
  data.header_map = map;

  data.transactions.reserve(doc.rows.size());
  for (const auto& row : doc.rows) {
    budget::Transaction tx;
    tx.fields = apply_header_map(row, map);
    data.transactions.push_back(std::move(tx));
  }

  return data;
}

HistoryData read_transaction_histories(const std::vector<std::string>& paths,
                                       const std::string& header_map_path) {
  HistoryData combined;
  for (const auto& path : paths) {
    auto data = read_transaction_history(path, header_map_path);
    if (combined.header_map.mapping.empty()) {
      combined.header_map = data.header_map;
    }
    combined.transactions.insert(
        combined.transactions.end(),
        std::make_move_iterator(data.transactions.begin()),
        std::make_move_iterator(data.transactions.end()));
  }
  return combined;
}

}  // namespace budget::io
