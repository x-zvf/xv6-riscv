/*!
 * \file
 * \brief test mmap with files
 */


#include <rt-test/assert.h>
#include <user/mmap.h>
#include <kernel/fcntl.h>

void main() {
  write(1, "F",1);
  printf("Begin test\n");
  //test invalid FD
  char *data = (char*) mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, 122, 0);
  assert(data == MAP_FAILED);

  // opened file
  int fd = open("./foo.txt", O_CREATE | O_RDWR);
  assert(fd >= 0);
  data = (char*) mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("mmap'ed file at %p\n", data);
  assert(data != MAP_FAILED);
  data[0] = 't';
  data[1] = 'e';
  data[2] = 's';
  data[3] = 't';
  printf("Here");
  munmap(data, 4096);
//  data = (char*) mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//  assert(data[0] == 't');
//  assert(data[1] == 'e');
//  assert(data[2] == 's');
//  assert(data[3] == 't');
//  munmap(data, 4096);
  close(fd);
  fd = open("./foo.txt", O_CREATE | O_RDWR);
  char buf[4096];
  int status = read(fd, buf, 4096);
  assert(buf[0] == 't');
  assert(status == 4096);
  exit(0);
}
