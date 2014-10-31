
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
  int fd, result;
  char buf[120];
  if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
    perror("opening file");
    return -1;
  }
  result = read (fd, &buf, 100);
  buf[result] = '\0';
  fprintf (stdout, "read back \"%s\"\n", buf); 
  close(fd);
  return 0;
}
