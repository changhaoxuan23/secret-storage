#ifndef STORAGE_HH_
#define STORAGE_HH_
#include "hardened_memory_allocator.hh"

class Storage final {
private:
  void *implementation;

public:
  Storage();
  Storage(const Storage &) = delete;
  Storage(Storage &&) = delete;
  Storage &operator=(const Storage &) = delete;
  Storage &operator=(Storage &&) = delete;
  ~Storage();

  auto add(const secured_string &&key, const secured_string &&value) -> bool;
  void update(const secured_string &&key, const secured_string &&value);
  [[nodiscard]] auto query(const secured_string &&key) const -> const secured_string *;
};
#endif