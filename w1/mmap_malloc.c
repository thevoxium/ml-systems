#include <stdio.h>
#include <sys/mman.h>

#define NUM 100000

void *get_ptr(size_t size) {
  void *ptr = mmap(NULL, size, PROT_WRITE | PROT_READ,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    perror("mmap() failed");
    return NULL;
  }
  return ptr;
}

void clean_ptr(void *ptr, size_t size) { munmap(ptr, size); }

int main() {
  size_t size = sizeof(int) * NUM;
  int *arr = (int *)get_ptr(size);
  if (arr == NULL) {
    perror("mmap is null");
    return -1;
  }
  arr[100] = 100;
  printf("%d \n", arr[100]);
  return 0;
}
