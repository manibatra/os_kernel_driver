
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
  int i, fd, result=0;
  char buf[10];
  if ((fd = open ("/dev/scullsort", O_RDWR | O_NONBLOCK)) == -1) {
    perror("opening file");
    return -1;
  }
  for(i = 0; i < 401; i++) {	
  	result = write (fd, "jihgfedcba",10);
  
 }

  close(fd);
  printf("Returned with  : %d\n", result);
  return 0;
}
