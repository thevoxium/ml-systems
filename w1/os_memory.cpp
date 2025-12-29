#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

typedef struct Mem {
  size_t total_bytes_allocated;
  size_t total_ptr_allocations;
  void *request_chunk(size_t size);
  bool release_chunk(void *ptr, size_t size);
  static size_t get_page_size();
  Mem() : total_bytes_allocated(0), total_ptr_allocations(0) {}
} Mem;

void *Mem::request_chunk(size_t size) {
  if (size <= 0) {
    perror("Size requested <= 0");
    return NULL;
  }
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    perror("Memory allocation failed");
    return NULL;
  }
  total_ptr_allocations++;
  total_bytes_allocated += size;
  return ptr;
}

bool Mem::release_chunk(void *ptr, size_t size) {
  if (ptr == NULL || ptr == MAP_FAILED) {
    perror("Failed to release chunk, invalid pointer");
    return false;
  }
  if (size <= 0) {
    perror("Failed to release chunk, invalid size");
    return false;
  }
  if (munmap(ptr, size) != 0) {
    perror("Error using munmap");
    return false;
  }
  total_ptr_allocations--;
  total_bytes_allocated -= size;
  return true;
}

size_t Mem::get_page_size() { return sysconf(_SC_PAGESIZE); }

void write_pattern_to_chunk(char *ptr, size_t size, size_t page_size) {
  char value = 0xAB;
  for (size_t offset = 0; offset < size; offset += page_size) {
    size_t chunk_size =
        (offset + page_size <= size) ? page_size : (size - offset);
    memset(ptr + offset, value, chunk_size);
    value++;
  }
}

bool verify_pattern(char *ptr, size_t size, size_t page_size) {
  char expected_value = 0xAB;
  for (size_t offset = 0; offset < size; offset += page_size) {
    size_t chunk_size =
        (offset + page_size <= size) ? page_size : (size - offset);
    for (size_t j = 0; j < chunk_size; j++) {
      char actual = ptr[offset + j];
      if (actual != expected_value) {
        return false;
      }
    }
    expected_value++;
  }
  return true;
}

bool test_allocation(Mem &allocator, size_t size, const char *test_name) {
  printf("Testing %s (size: %zu bytes)\n", test_name, size);

  void *ptr = allocator.request_chunk(size);
  if (ptr == NULL) {
    perror("allocation failed");
    return false;
  }

  write_pattern_to_chunk((char *)ptr, size, allocator.get_page_size());

  if (!verify_pattern((char *)ptr, size, allocator.get_page_size())) {
    perror("pattern verification failed");
    allocator.release_chunk(ptr, size);
    return false;
  }

  if (!allocator.release_chunk(ptr, size)) {
    perror("failed to release");
    return false;
  }

  printf("%s passed\n", test_name);
  return true;
}

int main() {
  Mem allocator;
  size_t page_size = Mem::get_page_size();

  bool all_passed = true;
  all_passed &= test_allocation(allocator, page_size, "Single page");
  all_passed &= test_allocation(allocator, page_size * 4, "4 pages");
  all_passed &= test_allocation(allocator, 1024, "1KB");
  all_passed &= test_allocation(allocator, 1024 * 1024, "1MB");
  all_passed &= test_allocation(allocator, 10 * 1024 * 1024, "10MB");
  all_passed &= test_allocation(allocator, 100 * 1024 * 1024, "100MB");
  all_passed &= test_allocation(allocator, page_size + 1, "Page + 1 byte");
  all_passed &= test_allocation(allocator, 500, "500 bytes");
  all_passed &= test_allocation(allocator, 1024UL * 1024UL * 1024UL, "1GB");
  all_passed &= test_allocation(allocator, 1, "1 byte");

  printf("All tests %s\n", all_passed ? "PASSED" : "FAILED");

  return all_passed ? 0 : -1;
}
