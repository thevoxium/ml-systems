
#include <cstddef>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

static inline uintptr_t align_up(uintptr_t p, size_t alignment) {
  return (p + alignment - 1) & ~(alignment - 1);
}

size_t next_power_of_two(size_t x) {
  if (x <= 1)
    return 1;
  return (size_t)1 << (sizeof(size_t) * 8 - __builtin_clzl(x - 1));
}

typedef struct Chunk {
  size_t order_;
  Chunk *next_;
  Chunk() : order_(0), next_(NULL) {}
} Chunk;

typedef struct BuddyAllocator {
  void *base_addr_;
  size_t max_memory_;
  size_t min_order_;
  size_t max_order_;
  Chunk **free_chunk_list_;
  void add_chunk(Chunk *chunk_ptr);
  bool find_chunk(Chunk *chunk_ptr);
  void remove_chunk(Chunk *chunk_ptr);
  void free_chunk(void *data_ptr);
  void *get_memory(size_t size);
  BuddyAllocator(size_t max_memory, size_t min_order, size_t max_order)
      : base_addr_(NULL), min_order_(min_order), max_order_(max_order),
        free_chunk_list_(NULL) {
    size_t min_size = sizeof(Chunk);
    size_t min_order_required = 0;
    while ((1ULL << min_order_required) < min_size) {
      min_order_required++;
    }
    if (min_order_ < min_order_required) {
      min_order_ = min_order_required;
    }
    size_t max_size = next_power_of_two(max_memory);
    size_t max_order_possible = 0;
    while ((1ULL << max_order_possible) < max_size) {
      max_order_possible++;
    }
    if (max_order_ > max_order_possible) {
      max_order_ = max_order_possible;
    }
    if (min_order_ > max_order_) {
      min_order_ = max_order_;
    }
    max_memory_ = next_power_of_two(max_memory);
    size_t list_size = max_order_ - min_order_ + 1;
    free_chunk_list_ = (Chunk **)malloc(list_size * sizeof(Chunk *));
    if (free_chunk_list_ == NULL) {
      perror("Error allocating free_chunk_list");
      return;
    }
    for (size_t i = 0; i < list_size; i++) {
      free_chunk_list_[i] = NULL;
    }
    base_addr_ = mmap(NULL, max_memory_, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base_addr_ == MAP_FAILED) {
      perror("Error using mmap");
      base_addr_ = NULL;
      return;
    }
    Chunk *full_block = (Chunk *)base_addr_;
    full_block->next_ = NULL;
    full_block->order_ = max_order_;
    add_chunk(full_block);
  }
  ~BuddyAllocator() {
    free(free_chunk_list_);
    free_chunk_list_ = NULL;
    if (base_addr_ && base_addr_ != MAP_FAILED) {
      munmap(base_addr_, max_memory_);
    }
    base_addr_ = NULL;
  }
} BuddyAllocator;

void BuddyAllocator::add_chunk(Chunk *chunk_ptr) {
  if (chunk_ptr == NULL) {
    return;
  }
  if (chunk_ptr->order_ < min_order_ || chunk_ptr->order_ > max_order_) {
    return;
  }
  size_t index = chunk_ptr->order_ - min_order_;
  chunk_ptr->next_ = free_chunk_list_[index];
  free_chunk_list_[index] = chunk_ptr;
}

bool BuddyAllocator::find_chunk(Chunk *chunk_ptr) {
  if (chunk_ptr == NULL) {
    return false;
  }
  if (chunk_ptr->order_ < min_order_ || chunk_ptr->order_ > max_order_) {
    return false;
  }
  size_t index = chunk_ptr->order_ - min_order_;
  Chunk *curr = free_chunk_list_[index];
  while (curr != NULL) {
    if (chunk_ptr == curr) {
      return true;
    }
    curr = curr->next_;
  }
  return false;
}

void BuddyAllocator::remove_chunk(Chunk *chunk_ptr) {
  if (chunk_ptr == NULL) {
    return;
  }
  if (chunk_ptr->order_ < min_order_ || chunk_ptr->order_ > max_order_) {
    return;
  }
  size_t index = chunk_ptr->order_ - min_order_;
  Chunk *prev = NULL;
  Chunk *curr = free_chunk_list_[index];
  while (curr) {
    if (curr == chunk_ptr) {
      if (prev) {
        prev->next_ = curr->next_;
      } else {
        free_chunk_list_[index] = curr->next_;
      }
      return;
    }
    prev = curr;
    curr = curr->next_;
  }
}

void *BuddyAllocator::get_memory(size_t size) {
  size_t required_size = next_power_of_two(size + sizeof(Chunk));
  if (required_size > (1ULL << max_order_) ||
      required_size < (1ULL << min_order_)) {
    perror("Size required is not in block range");
    return NULL;
  }
  size_t target_order = std::log2(required_size);
  for (size_t i = target_order; i <= max_order_; i++) {
    size_t index = i - min_order_;
    if (free_chunk_list_[index] == NULL) {
      continue;
    }
    Chunk *block = free_chunk_list_[index];
    remove_chunk(block);
    Chunk *curr = block;
    size_t curr_order = i;
    while (curr_order > target_order) {
      curr_order--;
      size_t half_size = (1ULL << curr_order);
      Chunk *buddy = (Chunk *)((char *)curr + half_size);
      buddy->order_ = curr_order;
      buddy->next_ = NULL;
      curr->order_ = curr_order;
      add_chunk(buddy);
    }
    return (void *)((char *)curr + sizeof(Chunk));
  }
  return NULL;
}

void BuddyAllocator::free_chunk(void *data_ptr) {
  if (data_ptr == NULL) {
    return;
  }
  Chunk *chunk_ptr = (Chunk *)((char *)data_ptr - sizeof(Chunk));
  size_t order = chunk_ptr->order_;
  while (order < max_order_) {
    Chunk *buddy =
        (Chunk *)((char *)base_addr_ +
                  (((char *)chunk_ptr - (char *)base_addr_) ^ (1ULL << order)));
    if (find_chunk(buddy) == false || buddy->order_ != chunk_ptr->order_) {
      break;
    }
    remove_chunk(buddy);
    if (buddy < chunk_ptr) {
      chunk_ptr = buddy;
    }
    order++;
    chunk_ptr->order_ = order;
  }
  add_chunk(chunk_ptr);
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

typedef struct HybridAllocator {
  size_t threshold_;
  LinearAllocator linear_allocator_;
  BuddyAllocator buddy_allocator_;

  HybridAllocator(size_t linear_max_size, size_t buddy_max_memory,
                  size_t buddy_min_order, size_t buddy_max_order)
      : threshold_(1024), linear_allocator_(linear_max_size),
        buddy_allocator_(buddy_max_memory, buddy_min_order, buddy_max_order) {}

  void *get_memory(size_t size) {
    if (size <= threshold_) {
      return linear_allocator_.get_memory(size);
    } else {
      return buddy_allocator_.get_memory(size);
    }
  }

  void free(void *ptr) {
    if (ptr >= linear_allocator_.base_address_ &&
        ptr < (char *)linear_allocator_.base_address_ +
                  linear_allocator_.max_size_) {
      return;
    } else {
      buddy_allocator_.free_chunk(ptr);
    }
  }
} HybridAllocator;

int main() {
  HybridAllocator hybrid(1024 * 1024, 4 * 1024 * 1024, 10, 22);

  void *small_ptr = hybrid.get_memory(512);
  void *large_ptr = hybrid.get_memory(2048);

  printf("Small allocation: %p\n", small_ptr);
  printf("Large allocation: %p\n", large_ptr);

  hybrid.free(small_ptr);
  hybrid.free(large_ptr);

  void *small_ptr2 = hybrid.get_memory(512);
  printf("Second small allocation: %p\n", small_ptr2);

  hybrid.linear_allocator_.reset();

  void *small_ptr3 = hybrid.get_memory(512);
  printf("After reset small allocation: %p\n", small_ptr3);

  return 0;
}
