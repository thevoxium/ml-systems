#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {

  char *file_path = "example.txt";
  int fd = open(file_path, O_RDWR);
  if (fd == -1) {
    perror("Error opening the file!");
    exit(EXIT_FAILURE);
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    perror("Error getting the file stat");
    exit(EXIT_FAILURE);
  }

  char *map = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    perror("Error mapping the file");
    close(fd);
    exit(EXIT_FAILURE);
  }

  map[0] = 'H';
  if (msync(map, sb.st_size, MS_SYNC) == -1) {
    perror("Error syncing to disc");
  }

  if (munmap(map, sb.st_size) == -1) {
    perror("Error un-mmapping the file");
  }
  close(fd);
  return 0;
}
