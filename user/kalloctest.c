#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define NCHILD 2
#define N 100000
#define SZ 4096

void test1(void);
void test2(void);
char buf[SZ];//4096  4KB

int
main(int argc, char *argv[])
{
  test1();
  test2();
  exit(0);
}

int ntas(int print)//0
{
  int n;
  char *c;

  //这里读取statistics文件中的数据到buf中 含有锁获取的数量信息
  if (statistics(buf, SZ) <= 0) {
    fprintf(2, "ntas: no stats\n");
  }
  //buf="tas_lock_acquires = 1234\n"
  c = strchr(buf, '=');//找到buf中=的位置
  n = atoi(c+2);//将=后面的字符串转换为整数
  if(print)
    printf("%s", buf);
  return n;//返回次数
}

void test1(void)
{
  void *a, *a1;
  int n, m;
  printf("start test1\n");  
  m = ntas(0);//锁获取失败的数量 打印出来 
  for(int i = 0; i < NCHILD; i++){//0 1
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid == 0){//子进程
      for(i = 0; i < N; i++) {//0 100000
        a = sbrk(4096);//分配4096字节内存
        *(int *)(a+4) = 1;//在a+4的位置写入1
        a1 = sbrk(-4096);//释放4096字节内存
        if (a1 != a + 4096) {//a1和a+4096不相等 说明分配失败
          printf("wrong sbrk\n");
          exit(-1);
        }
      }
      exit(-1);
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }
  printf("test1 results:\n");
  n = ntas(1);
  if(n-m < 10) 
    printf("test1 OK\n");
  else
    printf("test1 FAIL\n");
}

//
// countfree() from usertests.c
//
int
countfree()
{
  uint64 sz0 = (uint64)sbrk(0);
  int n = 0;

  while(1){
    uint64 a = (uint64) sbrk(4096);
    if(a == 0xffffffffffffffff){
      break;
    }
    // modify the memory to make sure it's really allocated.
    *(char *)(a + 4096 - 1) = 1;
    n += 1;
  }
  sbrk(-((uint64)sbrk(0) - sz0));
  return n;
}

void test2() {
  int free0 = countfree();//计算空闲页数
  int free1;
  int n = (PHYSTOP-KERNBASE)/PGSIZE;//xv6理论物理页数
  printf("start test2\n");  
  printf("total free number of pages: %d (out of %d)\n", free0, n);
  if(n - free0 > 1000) {
    printf("test2 FAILED: cannot allocate enough memory");
    exit(-1);
  }
  for (int i = 0; i < 50; i++) {
    free1 = countfree();//50次
    if(i % 10 == 9)
      printf(".");
    if(free1 != free0) {//查看是否有内存泄漏的情况
      printf("test2 FAIL: losing pages\n");
      exit(-1);
    }
  }
  printf("\ntest2 OK\n");  
}


