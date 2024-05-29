#ifndef HARDENED_MEMORY_ALLOCATOR_HH_
#define HARDENED_MEMORY_ALLOCATOR_HH_

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <unordered_map>

struct MemoryBlock;
class HardenedMemoryManager {
private:
  static constexpr size_t Align = sizeof(uintmax_t);

  static size_t       page_size;
  static MemoryBlock *list;
  static std::mutex   mutex;

  static void add_before(MemoryBlock *target, MemoryBlock *before);
  static void add_after(MemoryBlock *target, MemoryBlock *after);

  static void remove_from_list(MemoryBlock *target);

  static auto do_merge(MemoryBlock *first, MemoryBlock *second) -> bool;

  static void merge(MemoryBlock *target);

  static void add_to_list(MemoryBlock *entry);

  static void add_page();
  static void remove_page(MemoryBlock *entry);

  static auto find_suitable_entry(size_t size) -> MemoryBlock *;

public:
  HardenedMemoryManager() = delete;
  [[nodiscard]] static auto allocate(size_t size) -> void *;
  static void               deallocate(void *address);
  static void               shrink();
  static void               close();
};

template <typename T> class HardenedMemoryAllocator final {
public:
  using pointer            = T *;
  using const_pointer      = const T *;
  using void_pointer       = void *;
  using const_void_pointer = const void *;
  using value_type         = T;
  using size_type          = size_t;
  using difference_type    = ssize_t;
  template <typename U> struct rebind {
    using other = HardenedMemoryAllocator<U>;
  };

  using is_always_equal = std::true_type;

  [[nodiscard]] auto allocate(size_type n) -> pointer {
    return reinterpret_cast<pointer>(HardenedMemoryManager::allocate(n * sizeof(T)));
  }
  void deallocate(pointer p, size_type) { HardenedMemoryManager::deallocate(p); }
  auto operator==(const HardenedMemoryAllocator<T> &) -> bool { return true; }
  auto operator!=(const HardenedMemoryAllocator<T> &) -> bool { return false; }
};

using secured_string = std::basic_string<char, std::char_traits<char>, HardenedMemoryAllocator<char>>;

using secured_unordered_map = std::unordered_map<
  secured_string,
  secured_string,
  std::hash<secured_string>,
  std::equal_to<>,
  HardenedMemoryAllocator<std::pair<const secured_string, secured_string>>>;
#endif