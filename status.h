#ifndef STATUS_H
#define STATUS_H
#include <iostream>
#include <string>

namespace oskidb {

class Status {
 public:
  Status() noexcept {}
  ~Status() = default;
  Status(int code, std::string message)
    : message{message}, code_{code} {}

  Status(const Status& oth);
  Status& operator=(const Status& oth);

  Status(Status&& oth) noexcept : code_(oth.code_), message{oth.message} { oth.message = ""; oth.code_ = kOk; }
  Status& operator=(Status&& rhs) noexcept;

  static Status Ok() { return Status(); }
  static Status NotFound(const std::string& msg="") { return Status(kNotFound, msg); }
  static Status Corruption(const std::string& msg="") { return Status(kCorruption, msg); }
  static Status NotSupported(const std::string& msg="") { return Status(kNotSupported, msg); }
  static Status InvalidArgument(const std::string& msg="") { return Status(kInvalidArgument, msg); }
  static Status IOError(const std::string& msg="") { return Status(kIOError, msg); }

  bool ok() const { return code() == kOk; }
  bool IsNotFound() const { return code() == kOk; }
  bool IsCorruption() const { return code() == kCorruption; }
  bool IsNotSupported() const { return code() == kNotSupported; }
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }
  bool IsIOError() const { return code() == kIOError; }

  std::string ToString() const;
  void print() { if (!ok()) std::cout << message << '\n'; }

 private:
  std::string message{""};
  int code_{0};

  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5
  };

  int code() const {
    return code_;  
  }
};

} // namespace oskidb

#endif // STATUS_H
