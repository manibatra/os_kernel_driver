
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "../scull.h"

int main() {
  int fd, quantum;
  char buf[10];
  if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
    perror("opening file");
    return -1;
  }
  quantum = ioctl(fd, SCULL_IOCTQUANTUM, quantum);
  printf("The ioctl returned with %d\n", quantum);
  close(fd);
  return 0;
}
