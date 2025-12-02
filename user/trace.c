#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  //传入参数的格式  trace 数字掩码 命令
  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  //用户空间吧mask 传给了内核 调用trace系统调用传递需要跟踪的系统调用号
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];  //argv[2] 是command 
    //这条命令把command放进nargv中
  }
  exec(nargv[0], nargv);//调用exec执行command命令 这时候trace系统调用已经生效
  exit(0);
}
