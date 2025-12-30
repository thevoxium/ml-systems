#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>

size_t next_power_of_two(size_t size) {
  if (size == 0)
    return 1;
  size_t i = 0;
  while ((1ULL << i) < size) {
    i++;
  }
  return 1ULL << i;
}

typedef struct Chunk {
  size_t order_;
  Chunk *next_;
  Chunk() : order_(0), next_(NULL) {}
} Chunk;

typedef struct Allocator {
  void *base_addr_;
  size_t max_memory_;
  size_t min_order_;
  size_t max_order_;
  Chunk **free_chunk_list_;

  void add_chunk(Chunk *chunk_ptr);
  bool find_chunk(Chunk *chunk_ptr);
  void remove_chunk(Chunk *chunk_ptr);
  void free_chunk(Chunk *chunk_ptr);

  Chunk *get_memory(size_t size);

  Allocator(size_t max_memory, size_t min_order, size_t max_order)
      : base_addr_(NULL), min_order_(min_order), max_order_(max_order),
        free_chunk_list_(NULL) {
    max_memory_ = next_power_of_two(max_memory);
    size_t list_size = max_order_ - min_order_ + 1;
    free_chunk_list_ = (Chunk **)malloc(list_size * sizeof(Chunk *));
    if (free_chunk_list_ == NULL) {
      std::perror("Error allocating free_chunk_list");
      return;
    }
    for (size_t i = 0; i < list_size; i++) {
      free_chunk_list_[i] = NULL;
    }

    base_addr_ = mmap(NULL, max_memory_, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base_addr_ == MAP_FAILED) {
      std::perror("Error using mmap");
      base_addr_ = NULL;
      return;
    }

    Chunk *full_block = (Chunk *)malloc(sizeof(Chunk));
    if (full_block == NULL) {
      std::perror("Error allocating initial chunk");
      return;
    }
    full_block->next_ = NULL;
    full_block->order_ = max_order_;
    add_chunk(full_block);
  }

  ~Allocator() {
    for (size_t i = 0; i < max_order_ - min_order_ + 1; ++i) {
      Chunk *curr = free_chunk_list_[i];
      while (curr) {
        Chunk *next = curr->next_;
        free(curr);
        curr = next;
      }
    }
    free(free_chunk_list_);
    free_chunk_list_ = NULL;

    if (base_addr_ != NULL && base_addr_ != MAP_FAILED) {
      munmap(base_addr_, max_memory_);
    }
    base_addr_ = NULL;
  }

} Allocator;

void Allocator::add_chunk(Chunk *chunk_ptr) {
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

bool Allocator::find_chunk(Chunk *chunk_ptr) {
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

void Allocator::remove_chunk(Chunk *chunk_ptr) {
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

Chunk *Allocator::get_memory(size_t size) {
  size_t required_size = next_power_of_two(size);
  if (required_size > (1ULL << max_order_) ||
      required_size < (1ULL << min_order_)) {
    std::perror("Size required is not in block range");
    return NULL;
  }

  size_t target_order = std::log2(required_size);
  for (size_t i = target_order; i <= max_order_; i++) {
    size_t index = i - min_order_;
    if (free_chunk_list_[index]) {
      if (i == target_order) {
        Chunk *result = free_chunk_list_[index];
        remove_chunk(free_chunk_list_[index]);
        return result;
      } else {
        // todo: implement block spliting
      }
    }
  }
  return NULL;
}

void Allocator::free_chunk(Chunk *chunk_ptr) {
  if (chunk_ptr == NULL) {
    return;
  }
  add_chunk(chunk_ptr);
}

int main() {
  size_t max_memory = 1 << 20, min_order = 1, max_order = 20;
  Allocator alloc(max_memory, min_order, max_order);
  Chunk *block = alloc.get_memory(5);
  if (block) {
    printf("Found Block");
    alloc.free_chunk(block);
  }
  return 0;
}
