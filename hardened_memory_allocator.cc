#include "hardened_memory_allocator.hh"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <new>
#include <print>
#include <sys/mman.h>
#include <sys/random.h>
#include <unistd.h>

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

static bool allocated = false;

void HardenedMemoryManager::add_before(MemoryBlock *target, MemoryBlock *before) {
  if (before->last == nullptr) {
    list = target;
  } else {
    before->last->next = target;
  }
  target->next = before;
  target->last = before->last;
  before->last = target;
}
void HardenedMemoryManager::add_after(MemoryBlock *target, MemoryBlock *after) {
  target->last = after;
  target->next = after->next;
  if (after->next != nullptr) {
    after->next->last = target;
  }
  after->next = target;
}

void HardenedMemoryManager::remove_from_list(MemoryBlock *target) {
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

auto HardenedMemoryManager::do_merge(MemoryBlock *first, MemoryBlock *second) -> bool {
  if (reinterpret_cast<uint8_t *>(first) + first->size() == reinterpret_cast<uint8_t *>(second)) {
    first->size(first->size() + second->size());
    remove_from_list(second);
    return true;
  }
  return false;
}

void HardenedMemoryManager::merge(MemoryBlock *target) {
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

void HardenedMemoryManager::add_to_list(MemoryBlock *entry) {
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

void HardenedMemoryManager::add_page() {
  [[maybe_unused]] static bool _ = (HardenedMemoryManager::page_size = sysconf(_SC_PAGESIZE));

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
void HardenedMemoryManager::remove_page(MemoryBlock *entry) {
  assert(entry->is_leader() && entry->size() == page_size);
  remove_from_list(entry);
  getrandom(entry, page_size, 0);
  munlock(entry, page_size);
  munmap(entry, page_size);
}

auto HardenedMemoryManager::find_suitable_entry(size_t size) -> MemoryBlock * {
  MemoryBlock *entry = list;
  while (entry != nullptr) {
    if (entry->size() >= size) {
      break;
    }
    entry = entry->next;
  }
  return entry;
}

[[nodiscard]] auto HardenedMemoryManager::allocate(size_t size) -> void * {
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

  // setup cleanup
  if (!allocated) {
    atexit(HardenedMemoryManager::remove_all_pages);
    allocated = true;
  }

#ifndef NDEBUG
  std::println("  allocated [0x{:016x}]", reinterpret_cast<uintmax_t>(target) + sizeof(size_t));
#endif

  return reinterpret_cast<uint8_t *>(target) + sizeof(size_t);
}
void HardenedMemoryManager::deallocate(void *address) {
  auto entry = reinterpret_cast<MemoryBlock *>(reinterpret_cast<uint8_t *>(address) - sizeof(size_t));
  // randomly refill the content of block
  getrandom(address, entry->size() - sizeof(size_t), 0);
  add_to_list(entry);
#ifndef NDEBUG
  std::println("deallocated [0x{:016x}]", reinterpret_cast<uintmax_t>(address));
#endif
}
void HardenedMemoryManager::remove_all_pages() {
  while (list != nullptr) {
    if (list->is_leader()) {
      remove_page(list);
    } else {
      remove_from_list(list);
      std::println(stderr, "non-leader entry found during final cleanup, memory leak or improper merge?");
    }
  }
}
MemoryBlock *HardenedMemoryManager::list = nullptr;
size_t       HardenedMemoryManager::page_size;
std::mutex   HardenedMemoryManager::mutex{};