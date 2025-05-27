#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define BUFFER_SIZE 100
int main(int argc, char* argv[])
{
    int pf2s[2],ps2f[2];
    char buffer='p';
    // 创建管道
    if (pipe(pf2s) == -1) {
        fprintf(2,"create pipe f2s error\n");
        exit(1);
    }
    // 创建管道
    if (pipe(ps2f) == -1) {
        fprintf(2,"create pipe f2s error\n");
        exit(1);
    }
    int pid=fork();
    if (pid < 0) {
        fprintf(2, "fork() error!\n");
        close(pf2s[0]);
        close(pf2s[1]);
        close(ps2f[0]);
        close(ps2f[1]);
        exit(1);
    }
    else if(pid!=0)
    {
        // 父进程
        close(pf2s[0]);  // 关闭读端
        close(ps2f[1]);  // 关闭写端
        if(write(pf2s[1], "f", 1)!=sizeof(char)) // 向管道写入数据
        {
            fprintf(2,"father write error\n");
            close(pf2s[1]);  // 关闭写端
            close(ps2f[0]);  // 关闭读端
            exit(1);
        }
        close(pf2s[1]);  // 关闭写端

        if(read(ps2f[0], &buffer, 1)!=sizeof(char)) // 从管道读取数据
        {
            fprintf(2,"read from son error\n");
            close(ps2f[0]);  // 关闭读端
            exit(1);
        }
        printf("%d: received pong\n", getpid()); // 打印数据
        wait(0); // 等待子进程结束
    }
    else
    {
        //子进程
        close(pf2s[1]);  // 关闭写端
        close(ps2f[0]);  // 关闭读端
        if(read(pf2s[0], &buffer, 1)!=sizeof(char)) // 向管道写入数据
        {
            fprintf(2,"son write error\n");
            close(pf2s[0]);  // 关闭写端
            close(ps2f[1]);  // 关闭读端
            exit(1);
        }
        printf("%d: received ping\n", getpid()); // 打印数据
        close(pf2s[0]);  // 关闭读端

        if(write(ps2f[1], "s", 1)!=sizeof(char)) // 向管道写入数据
        {
            fprintf(2,"son write error\n");
            close(ps2f[1]);  // 关闭写端
            exit(1);
        }
        close(ps2f[1]);  // 关闭写端
        exit(0); // 结束子进程
    }

    exit(0);
}
