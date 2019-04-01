#ifndef OSKIDB_H
#define OSKIDB_H

#include "MurmurHash3.h"
#include "options.h"
#include "status.h"

namespace oskidb {

constexpr int NUM_TABLES = 256;
constexpr char MAGIC[] = "GOBEARS!";
constexpr int MAGIC_SIZE = 8;
constexpr int VERSION = 1;
constexpr int BYTEORDER_CHECK = 0x62445371;

struct tableref_t {
  int32_t offset;
  uint32_t num;        
};

struct bucket {
  uint32_t hash;
  uint32_t offset;
  bucket() = default;
  bucket(uint32_t h, uint32_t o)
    : hash(h), offset(o) {
  }
};

struct hashtable {
  uint32_t num;
  std::vector<bucket> buckets;
};

inline uint32_t hash_string(std::string str) {
  uint32_t result;
  uint32_t seed = 0xcafe;
  const char *data = str.data();
  MurmurHash3_x86_32(data, str.size(), seed, &result);
  return result;
}

inline uint32_t get_data_begin() {
  return (MAGIC_SIZE + (3 * sizeof(uint32_t)) + sizeof(tableref_t) * NUM_TABLES);
}

} // namespace oskidb

#endif /* OSKIDB_H */