#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
statistics(void *buf, int sz)
{
  int fd, i, n;
  
  //statistics文件是一个联通内核的文件
  fd = open("statistics", O_RDONLY);//只读模式打开fd文件
  if(fd < 0) {
      fprintf(2, "stats: open failed\n");
      exit(1);
  }
  //读取sz字节的数据到buf中
  for (i = 0; i < sz; ) {
    if ((n = read(fd, buf+i, sz-i)) < 0) {
      break;
    }
    i += n;
  }
  close(fd);//关闭文件
  return i;
}
