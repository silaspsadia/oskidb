#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <stdint.h>
#include "./oskidb.h"


namespace oskidb {

class DbWriter {
public:
  DbWriter(const std::string &filename)
    : m_os{filename, std::ofstream::out} {
    m_begin = m_os.tellp();
    m_cur = get_data_begin();
    m_os.seekp(m_begin + m_cur);
  }

  ~DbWriter() {
    close();
  }

  Status put(const std::string &key, const std::string &value, WriteOptions &wo) {
    write_uint32(key.size());
    write_string(key);
    write_uint32(value.size());
    write_string(value);
    uint32_t hv = hash_string(key);
    hashtable &ht = m_ht[hv % NUM_TABLES];
    ht.buckets.push_back(bucket(hv, m_cur));
    m_cur += sizeof(uint32_t) + key.size() + sizeof(uint32_t) + value.size();
    return Status::Ok();
  }

private:
  std::ofstream m_os;
  uint32_t m_begin;
  uint32_t m_cur;
  hashtable m_ht[NUM_TABLES];

  Status close() {
    if (m_begin + m_cur != static_cast<uint32_t>(m_os.tellp()))
      return Status::IOError("Inconsistent stream offset");
    for (auto i = 0; i < NUM_TABLES; ++i) {
      hashtable &ht = m_ht[i];
      if (!ht.buckets.empty()) {
        auto n = ht.buckets.size() * 2;
        std::vector<bucket> dst{n};
        for (auto &element : ht.buckets) {
          auto k = (element.hash>>8) % n;
          while (dst[k].offset != 0) 
            k = (k+1) % n;
          dst[k].hash = element.hash;
          dst[k].offset = element.offset;
        }
        for (auto k = 0; k < n; ++k) {
          write_uint32(dst[k].hash);
          write_uint32(dst[k].offset);
        }
      }
    }

    uint32_t offset = static_cast<uint32_t>(m_os.tellp());
    m_os.seekp(m_begin);
    m_os.write(MAGIC, MAGIC_SIZE);
    write_uint32(offset - m_begin);
    write_uint32(VERSION);
    write_uint32(BYTEORDER_CHECK);

    for (auto i = 0; i < NUM_TABLES; ++i) {
      write_uint32(m_ht[i].buckets.empty() ? 0 : m_cur);
      write_uint32(m_ht[i].buckets.size() * 2);
      m_cur += sizeof(uint32_t) * 2 * m_ht[i].buckets.size() * 2;
    }
    m_os.seekp(offset);
    return Status::Ok();
  }

  inline void write_uint32(uint32_t value) {
    m_os.write(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  inline void write_string(const std::string& str) {
    m_os.write(reinterpret_cast<const char *>(str.data()), str.size());
  }
}; // class DbWriter

class DbReader {
public:
  DbReader(const std::string &filename) {
    open(filename);
  }

  ~DbReader() {
    close();
  }

  Status open(const std::string &filename) {
    std::ifstream ifs{filename, std::ifstream::in};
    std::istream::pos_type offset = ifs.tellg();
    char magic[8], size[4];
    ifs.read(magic, 8);
    if (ifs.fail())
      return Status::IOError("Failed to read magic value from DB file.");
    if (std::strncmp(magic, MAGIC, 8) != 0)
      return Status::IOError("Bad magic value read from DB file.");
    ifs.read(size, 4);
    if (ifs.fail())
      return Status::IOError("Failed to read size from DB file.");
    uint32_t block_size = read_uint32(reinterpret_cast<uint8_t*>(size));
    uint8_t* block = new uint8_t[block_size];
    ifs.seekg(offset, std::ios_base::beg);
    if (ifs.fail())
      return Status::IOError("Failed to seek to memory image in DB file.");
    ifs.read(reinterpret_cast<char*>(block), block_size);
    if (ifs.fail())
      return Status::IOError("Failed to read memory image from DB file.");
    return load(block, block_size, true);
  }

  Status load(const void *buffer, size_t size, bool own = false) {
    // TODO: create a ByteArray class (stop dealing with reinterpret_cast's and uint8_t)
    const uint8_t *p = reinterpret_cast<const uint8_t*>(buffer);
    if (size < get_data_begin())
      return Status::Corruption("Memory image is smaller than a chunk header.");
    if (memcmp(p, MAGIC, 8) != 0)
      return Status::Corruption("Bad magic value read from memory image.");
    p += 8;
    uint32_t csize = read_uint32(p);
    p += sizeof(uint32_t);
    uint32_t version = read_uint32(p);
    p += sizeof(uint32_t);
    uint32_t byteorder = read_uint32(p);
    p += sizeof(uint32_t);
    if (byteorder != BYTEORDER_CHECK)
      return Status::Corruption("Inconsistent byte order read from memory image.");
    if (size < csize)
      return Status::Corruption("Memory image is smaller than chunk size.");
    m_buffer = reinterpret_cast<const uint8_t*>(buffer);
    m_size = size;
    m_own = own;
    m_n = 0;
    const tableref_t* ref = reinterpret_cast<const tableref_t*>(p);
    for (auto i = 0; i < NUM_TABLES; ++i) {
      if (ref[i].offset) {
        m_ht[i].buckets = std::vector<bucket>{};
        const bucket *bucket_array = reinterpret_cast<const bucket*>(m_buffer + ref[i].offset);
        m_ht[i].buckets = std::vector<bucket>(bucket_array, bucket_array + ref[i].num);
        m_ht[i].num = ref[i].num;
      } else {
        m_ht[i].buckets = std::vector<bucket>{};
        m_ht[i].num = 0;
      }
      m_n += ref[i].num / 2;
    }
    return Status::Ok();
  }

  Status close() noexcept {
    if (m_buffer != nullptr)
      delete[] m_buffer;
    m_buffer = nullptr;
    m_size = 0;
    m_n = 0;
    return Status::Ok();
  }

  Status get(const std::string &key, std::string *value, ReadOptions &ro) {
    uint32_t hv = hash_string(key);
    const hashtable &ht = m_ht[hv % NUM_TABLES];
    if (ht.num && !ht.buckets.empty()) {
      auto n = ht.num;
      auto k = (hv>>8) % n;
      const bucket *p;
      p = &(ht.buckets[k]); 
      while (p = &(ht.buckets[k]), p->offset) {
        if (p->hash == hv) {
          const uint8_t *q = m_buffer + p->offset;
          if (read_uint32(q) == key.size() &&
            memcmp(key.data(), q + sizeof(uint32_t), key.size()) == 0) {
            q += sizeof(uint32_t) + key.size();
            uint32_t data_size = read_uint32(q);
            q += sizeof(uint32_t);
            *value = std::string(reinterpret_cast<char *>(const_cast<uint8_t *>(q)), data_size);
            return Status::Ok();
          }
        }
        k = (k+1) % n;
      }
    }
    return Status::NotFound();
  }

private:
  const uint8_t* m_buffer; 
  size_t m_size;
  bool m_own;
  hashtable m_ht[NUM_TABLES];
  size_t m_n;

  inline uint32_t read_uint32(const uint8_t* p) const {
    return *reinterpret_cast<const uint32_t*>(p);
  }
}; // class DbReader

} //namespace oskidb






