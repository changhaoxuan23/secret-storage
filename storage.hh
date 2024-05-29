#ifndef STORAGE_HH_
#define STORAGE_HH_
#include "hardened_memory_allocator.hh"

class Storage final {
private:
  void *implementation;

public:
  Storage();
  Storage(const Storage &)                     = delete;
  Storage(Storage &&)                          = delete;
  auto operator=(const Storage &) -> Storage & = delete;
  auto operator=(Storage &&) -> Storage      & = delete;
  ~Storage();

  auto               add(const secured_string &&key, const secured_string &&value) -> bool;
  void               update(const secured_string &&key, const secured_string &&value);
  [[nodiscard]] auto query(const secured_string &&key) const -> const secured_string *;
  auto               remove(const secured_string &&key) -> size_t;
};
#endif