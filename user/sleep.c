#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char *argv[])
{
    if (argc!=2)
    {
        fprintf(2,"Usage: sleep <time>\n");
        exit(1);
    }
    int time=atoi(argv[1]);
    if (time<=0)
    {
        fprintf(2,"Usage: sleep <time>  time need > 0\n");
        exit(1);
    }
    sleep(time);//调用的kernel中的sleep函数
    exit(0);
}