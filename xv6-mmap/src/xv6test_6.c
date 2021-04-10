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
Test that we split unmapped large region into two regions
*/

void test() {
  void* res1=0;
  res1 = mmap(res1, 0x2000, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res1<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good using address = %p\n", res1);

  int rv1 = munmap(res1, 0x2000);
  if (rv1 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");


  void* res2=0;
  res2 = mmap(res2, 0x1000, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res2<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good using address = %p\n", res2);


  void* res3=0;
  res3 = mmap(res3, 0x1000, 0/*prot*/, 0/*flags*/, -1/*fd*/, 0/*offset*/);
  if (res2<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good using address = %p\n", res3);

  int rv2 = munmap(res2, 0x1000);
  if (rv2 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");

  int rv3 = munmap(res3, 0x1000);
  if (rv3 < 0) {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : munmap good\n");

  if ((uint)res2 != 0x3000 || (uint)res3 != 0x4000) {
    printf(1, "XV6_TEST_OUTPUT : failed\n");
    return;
  }
  printf(1, "XV6_TEST_OUTPUT : success\n");
}


int
main(int argc, char *argv[])
{
  test();
  exit();
}
