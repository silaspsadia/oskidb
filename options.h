#ifndef OPTIONS_H
#define OPTIONS_H
#include <iostream>
#include <string>

namespace oskidb {

struct Options {
  bool create_if_missing{false};
  bool error_if_exists{false};
  int max_open_files{1000};
  size_t max_file_size{2<<20};
  size_t block_size{4096};
  Options() = default;
};

struct ReadOptions {
  bool verify_checksum{false};
  bool fill_cache{true};
  ReadOptions() = default;
};

struct WriteOptions {
  bool sync{false};
  WriteOptions() = default;
};

} // namespace oskidb

#endif // OPTIONS_H