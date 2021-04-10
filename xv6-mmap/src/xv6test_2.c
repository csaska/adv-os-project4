#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

/*
Test that we use first free space when no hint given
*/

void test() {
  int size =  10;

  void* res1=0;
  res1 = mmap(res1, size, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res1<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good using address = %p\n", res1);
  void* res2=0;
  res2 = mmap(res2, size, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res2<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good using address = %p\n", res2);


  int rv1 = munmap(res1, size);
  if (rv1 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");
  int rv2 = munmap(res2, size);
  if (rv2 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");


  void* res3=0;
  res3 = mmap(res3, size, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res2<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  if (res3 == res1)
    printf(1, "XV6_TEST_OUTPUT : mmap good, used first free when no hint\n");
  else
    printf(1, "XV6_TEST_OUTPUT : mmap bad, didn't honor hint\n");

  int rv3 = munmap(res3, size);
  if (rv3 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");
}


int
main(int argc, char *argv[])
{
  test();
  exit();
}
