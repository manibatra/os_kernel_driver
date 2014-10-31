
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
  int fd, result;
  char buf[10];
  if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
    perror("opening file");
    return -1;
  }
  printf("About to write : jihgfedcba\n");
  if ((result = write (fd, "jihgfedcba",10)) != 10) {
    perror("writing");
    return -1;
  }

  close(fd);
  printf("Writeless returned\n");
  return 0;
}
