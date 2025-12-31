#include <cstddef>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

static inline uintptr_t align_up(uintptr_t p, size_t alignment) {
  return (p + alignment - 1) & ~(alignment - 1);
}

typedef struct LinearAllocator {
  size_t max_size_;
  void *base_address_;
  void *curr_address_;

  void *get_memory(size_t size);
  void reset();

  LinearAllocator(size_t max_size) : max_size_(max_size) {
    base_address_ = mmap(NULL, max_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base_address_ == MAP_FAILED) {
      perror("Mmap allocation failed, base_address_ = NULL");
      base_address_ = NULL;
    }
    curr_address_ = base_address_;
  }

  ~LinearAllocator() {
    if (base_address_ && base_address_ != MAP_FAILED) {
      munmap(base_address_, max_size_);
    }
  }
} LinearAllocator;

void *LinearAllocator::get_memory(size_t size) {
  if (!base_address_ || size == 0) {
    return NULL;
  }

  uintptr_t curr = (uintptr_t)curr_address_;
  uintptr_t aligned = align_up(curr, alignof(std::max_align_t));

  size_t used = aligned - (uintptr_t)base_address_;

  if (size > max_size_ - used) {
    return NULL;
  }

  void *result = (void *)aligned;
  curr_address_ = (void *)(aligned + size);

  return result;
}

void LinearAllocator::reset() { curr_address_ = base_address_; }

int main() {
  LinearAllocator alloc(1024);
  void *ptr = alloc.get_memory(10);
  if (ptr == NULL) {
    return 1;
  }
  void *ptr1 = alloc.get_memory(100);
  if (ptr1 == NULL) {
    return 1;
  }

  printf("difference: %zu bytes\n", (size_t)((uintptr_t)ptr1 - (uintptr_t)ptr));

  alloc.reset();

  return 0;
}
