#include "storage.hh"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <print>
#include <string>
#include <sys/mman.h>
#include <sys/random.h>
#include <unistd.h>
#include <unordered_map>
class HardenedMemoryManager {
private:
  static constexpr size_t Align = sizeof(uintmax_t);
  struct MemoryBlock {
  private:
    size_t size_{0};

  public:
    MemoryBlock *last{nullptr};
    MemoryBlock *next{nullptr};

    inline auto size() -> size_t { return this->size_ & ~static_cast<size_t>(1); }
    inline void size(size_t value) { this->size_ = value | (this->size_ & 1); }
    inline void mark_as_leader() { this->size_ |= 1; }
    inline auto is_leader() -> bool { return this->size_ & 1; }
  };

  const static size_t page_size;
  static MemoryBlock *list;
  static std::mutex   mutex;

  static void add_before(MemoryBlock *target, MemoryBlock *before) {
    if (before->last == nullptr) {
      list = target;
    } else {
      before->last->next = target;
    }
    target->next = before;
    target->last = before->last;
    before->last = target;
  }
  static void add_after(MemoryBlock *target, MemoryBlock *after) {
    target->last = after;
    target->next = after->next;
    if (after->next != nullptr) {
      after->next->last = target;
    }
    after->next = target;
  }

  static void remove_from_list(MemoryBlock *target) {
    if (target->last == nullptr) {
      assert(list == target);
      list = target->next;
    } else {
      target->last->next = target->next;
    }
    if (target->next != nullptr) {
      target->next->last = target->last;
    }
  }

  static auto do_merge(MemoryBlock *first, MemoryBlock *second) -> bool {
    if (reinterpret_cast<uint8_t *>(first) + first->size() == reinterpret_cast<uint8_t *>(second)) {
      first->size(first->size() + second->size());
      remove_from_list(second);
      return true;
    }
    return false;
  }

  static void merge(MemoryBlock *target) {
    if (!target->is_leader() && target->last != nullptr) {
      auto last = target->last;
      if (do_merge(target->last, target)) {
        target = last;
      }
    }
    if (target->next != nullptr && !target->next->is_leader()) {
      do_merge(target, target->next);
    }
  }

  static void add_to_list(MemoryBlock *entry) {
    if (list == nullptr) {
      list        = entry;
      entry->last = nullptr;
      entry->next = nullptr;
      return;
    }
    MemoryBlock *target = list;
    while (target->next != nullptr) {
      if (target > entry) {
        break;
      }
      target = target->next;
    }
    if (target > entry) {
      add_before(entry, target);
    } else {
      add_after(entry, target);
    }
    merge(entry);
  }

  static void add_page() {
    void *page = mmap(
      nullptr,
      page_size,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | MAP_NORESERVE,
      -1,
      0
    );
    if (page == MAP_FAILED) {
      throw std::bad_alloc();
    }
    do {
      if (mlock(page, page_size) == -1) {
        break;
      }
      auto entry = reinterpret_cast<MemoryBlock *>(page);
      entry->size(page_size);
      entry->mark_as_leader();
      add_to_list(entry);
      return;
    } while (false);
    munmap(page, page_size);
    throw std::bad_alloc();
  }
  static void remove_page(MemoryBlock *entry) {
    assert(entry->is_leader() && entry->size() == page_size);
    remove_from_list(entry);
    getrandom(entry, page_size, 0);
    munlock(entry, page_size);
    munmap(entry, page_size);
  }

  static auto find_suitable_entry(size_t size) -> MemoryBlock * {
    MemoryBlock *entry = list;
    while (entry != nullptr) {
      if (entry->size() >= size) {
        break;
      }
      entry = entry->next;
    }
    return entry;
  }

public:
  HardenedMemoryManager() = delete;
  [[nodiscard]] static auto allocate(size_t size) -> void * {
    std::println("allocating {} bytes", size);
    // add to size so that it takes the hidden fields into account
    size += sizeof(size_t);
    // size must be large enough
    size = std::max(size, sizeof(MemoryBlock));
    // size must be aligned
    if (size % Align != 0) {
      size = (size / Align + 1) * Align;
    }

    MemoryBlock *target = nullptr;

    { // we need a lock from now on till we detached the target entry off the list
      std::lock_guard<std::mutex> lock(mutex);

      target = find_suitable_entry(size);
      if (target == nullptr) {
        add_page();
        target = find_suitable_entry(size);
      }
      if (target == nullptr) {
        // likely allocating block with size of more than one page
        //  this is not yet implemented
        throw std::bad_alloc();
      }
      remove_from_list(target);
    }

    if (target->size() - size >= sizeof(MemoryBlock)) {
      // we split the block since the remaining part will have enough size
      auto entry = reinterpret_cast<MemoryBlock *>(reinterpret_cast<uint8_t *>(target) + size);
      entry->size(target->size() - size);
      { // we need a lock to attach the remaining block back
        std::lock_guard<std::mutex> lock(mutex);
        add_to_list(entry);
      }
      target->size(size);
    }

    // fill the allocated part with 0x42
    memset(reinterpret_cast<uint8_t *>(target) + sizeof(size_t), 0x42, target->size() - sizeof(size_t));

    std::println(
      "又来了 0x{:016x}", reinterpret_cast<uintptr_t>(reinterpret_cast<uint8_t *>(target) + sizeof(size_t))
    );

    return reinterpret_cast<uint8_t *>(target) + sizeof(size_t);
  }
  static void deallocate(void *address) {
    std::println("又走了 0x{:016x}", reinterpret_cast<uintptr_t>(address));
    auto entry = reinterpret_cast<MemoryBlock *>(reinterpret_cast<uint8_t *>(address) - sizeof(size_t));
    // randomly refill the content of block
    getrandom(address, entry->size() - sizeof(size_t), 0);
    add_to_list(entry);
  }
};
HardenedMemoryManager::MemoryBlock *HardenedMemoryManager::list      = nullptr;
const size_t                        HardenedMemoryManager::page_size = sysconf(_SC_PAGESIZE);
std::mutex                          HardenedMemoryManager::mutex{};

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

using secured_string        = std::basic_string<char, std::char_traits<char>, HardenedMemoryAllocator<char>>;
using secured_unordered_map = std::unordered_map<
  secured_string,
  secured_string,
  std::hash<secured_string>,
  std::equal_to<>,
  HardenedMemoryAllocator<std::pair<const secured_string, secured_string>>>;

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
    std::ranges::for_each(aa, [](char &c) { c = toupper(c); });
    b.insert(std::make_pair(a, aa));
    a.clear();
  }
  for (const auto &[a, aa] : b) {
    std::cout << a << " -> " << aa << '\n';
  }
  return 0;
}