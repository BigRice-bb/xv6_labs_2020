#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1


// void transmit_right(int lpipe[2],int rpipe[2],int first)
// {
//     int num;
//     //读出左边管道的所有数据  --将当前素数的倍数过滤掉传入下一个管道
//     while (read(lpipe[RD], &num, sizeof(num)) == sizeof(num))
//     {
//         if (num % first != 0)
//         {
//             //不是倍数 传输给右边的管道
//             write(rpipe[WR], &num, sizeof(num));
//         }
//     }
//     close(lpipe[RD]);
//     close(rpipe[WR]);
// }

// //从左边的管道读 满足条件传输给右边的管道
// void primes(int lpipe[2])
// {
//     close(lpipe[WR]);//关闭写端
//     int first;
//     if (read(lpipe[RD], &first, sizeof(first)) == sizeof(first))
//     {
//         //读到了一个数据--这个数据从左边传过来必然为素数
//         printf("prime %d\n", first);
//     }
//     else
//     {
//         //printf("no prime\n");
//         //没有读到数据 说明没有数据了 退出子进程
//         close(lpipe[RD]);
//         exit(0);
//     }
//     int rpipe[2];//往右边写的管道
//     if (pipe(rpipe) < 0)
//     {
//         fprintf(2, "pipe error\n");
//         exit(1);
//     }
//     transmit_right(lpipe,rpipe,first);//传输给右边的管道
//     //递归调用
//     if (fork() == 0)
//     {
//         //子进程
//         primes(rpipe);
//     }
//     else
//     {
//         //父进程
//         close(rpipe[RD]);
//         wait(0);
//     }

// }
// int main(int argc, char const *argv[])
// {
//     //第一步创建管道
//     int p[2];
//     if (pipe(p) < 0)
//     {
//         fprintf(2, "pipe error\n");
//         exit(1);
//     }
//     //第二步写入初始数据
//     for (int i = 2; i <= 35; i++)
//     {
//         write(p[WR], &i, sizeof(i));//不断地往管道里写
//     }
//     if (fork()==0)
//     {
//         //递归子进程判断素数  打印  传递给右边
//         primes(p);
//     }
//     else
//     {
//         //父进程关闭读端
//         close(p[RD]);
//         //父进程关闭写端
//         close(p[WR]);
//         //父进程等待子进程结束
//         wait(0);
//     }
//   exit(0);
// }


void process(int lpipe[2])
{
    close(lpipe[WR]);
    int first;
    if (read(lpipe[RD], &first, sizeof(first)) == sizeof(first))
    {
        //读到了一个数据--这个数据从左边传过来必然为素数
        printf("prime %d\n", first);
    }
    else
    {
        //没有读到数据 说明没有数据了 退出子进程
        close(lpipe[RD]);
        exit(0);
    }
    //接下来循环读出所有数据 过滤写入右
    int rpipe[2];
    if(pipe(rpipe)<0)
    {
        fprintf(2,"rpipe error\n");
        exit(1);
    }
    //close(rpipe[RD]);
    int num;
    while (read(lpipe[RD], &num, sizeof(num)) == sizeof(num))
    {
        if (num % first != 0)
        {
            //不是倍数 传输给右边的管道
            write(rpipe[WR], &num, sizeof(num));
        }
    }
    
    int pid=fork();
    if(pid<0)
    {
        fprintf(2,"fork error\n");
        exit(1);
    }
    else if (pid==0)
    {
        //子进程
        close(rpipe[WR]);
        process(rpipe);
    }
    else
    {
        //父进程
        close(rpipe[RD]);
        close(lpipe[RD]);
        close(rpipe[WR]);
        wait(0);
    }
}
//左边管道拿出全部数据 过滤掉第一个数据的倍数 传给右边管道
int main(int argc,char** argv)
{
    int lpipe[2];
    if(pipe(lpipe)<0)
    {
        fprintf(2,"lpipe error\n");
        exit(1);
    }
    //写入数据
    for (int i = 2; i <= 35; i++)
    {
        if(write(lpipe[WR],&i,sizeof(i))!=sizeof(i))
        {
            fprintf(2,"lpipe write error\n");
            exit(1);
        }
    }
    int pid=fork();
    if (pid<0)
    {
        fprintf(2,"fork error\n");
        exit(1);
    }
    else if (pid==0)
    {
        //子进程--从左边读 打印第一个数  过滤倍数  传给右 递归
        close(lpipe[WR]);
        process(lpipe);
    }
    else
    {
        //父进程--关闭管道等待之进程退出
        close(lpipe[RD]);
        close(lpipe[WR]);
        wait(0);
        exit(0);
    }
    exit(0);
}