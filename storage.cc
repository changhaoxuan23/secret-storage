#include "storage.hh"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>
#include <unordered_map>

class StorageImplementation {
private:
  secured_unordered_map map;
  mutable std::mutex    mutex;

public:
  auto add(const secured_string &&key, const secured_string &&value) -> bool {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->map.insert(std::make_pair(key, value)).second;
  }
  void update(const secured_string &&key, const secured_string &&value) {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->map.insert_or_assign(key, value);
  }
  [[nodiscard]] auto query(const secured_string &&key) const -> const secured_string * {
    std::lock_guard<std::mutex> lock(this->mutex);
    const auto                  iterator = this->map.find(key);
    if (iterator == this->map.cend()) {
      return nullptr;
    }
    return &iterator->second;
  }
  auto remove(const secured_string &&key) -> size_t {
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->map.erase(key);
  }
};

Storage::Storage() { this->implementation = new StorageImplementation(); }
Storage::~Storage() { delete reinterpret_cast<StorageImplementation *>(this->implementation); }
auto Storage::add(const secured_string &&key, const secured_string &&value) -> bool {
  return reinterpret_cast<StorageImplementation *>(this->implementation)
    ->add(std::forward<const secured_string>(key), std::forward<const secured_string>(value));
}
void Storage::update(const secured_string &&key, const secured_string &&value) {
  return reinterpret_cast<StorageImplementation *>(this->implementation)
    ->update(std::forward<const secured_string>(key), std::forward<const secured_string>(value));
}
[[nodiscard]] auto Storage::query(const secured_string &&key) const -> const secured_string * {
  return reinterpret_cast<StorageImplementation *>(this->implementation)
    ->query(std::forward<const secured_string>(key));
}
auto Storage::remove(const secured_string &&key) -> size_t {
  return reinterpret_cast<StorageImplementation *>(this->implementation)
    ->remove(std::forward<const secured_string>(key));
}

__attribute__((weak)) auto main() -> int {
  secured_string        a;
  secured_unordered_map b;
  for (size_t i = 0; i < 5; i++) {
    while (true) {
      auto c = std::cin.get();
      if (c == '\n') {
        break;
      }
      a.push_back(static_cast<char>(c));
    }
    auto aa = a;
    std::ranges::for_each(aa, [](char &c) { c = static_cast<char>(toupper(c)); });
    b.insert(std::make_pair(a, aa));
    a.clear();
  }
  for (const auto &[a, aa] : b) {
    std::cout << a << " -> " << aa << '\n';
  }
  return 0;
}