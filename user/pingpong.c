#include "kernel/types.h"  //数据类型标准封装
#include "user/user.h" //系统调用接口


/*
*任务：
*1.创建两个进程，一个进程发送ping，另一个进程发送pong
*2.进程间利用管道进行通信
*3.通信间隔为1秒
*/

int main(int argc,char ** argv)
{
    int p1[2];//p1用来子发 父收
    if(pipe(p1) < 0){
        fprintf(2, "pipe p1 failed\n");
        exit(1);
    }
    int p2[2];//p2用来父发 子收
    if(pipe(p2) < 0){
        fprintf(2, "pipe p2 failed\n");
        exit(1);
    }
    char buf[5]={0};
    //创建两个进程
    int pid=fork();
    if(pid < 0){
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if(pid==0)
    {
        close(p1[0]);
        close(p2[1]);
        //子进程
        while(1)
        {
            if(write(p1[1],"ping",5) != 5){
                fprintf(2, "child: write failed\n");
                break;
            }
            sleep(1);
            if(read(p2[0],buf,5) != 5){
                fprintf(2, "child: read failed\n");
                break;
            }
            printf("child %d: received %s\n",getpid(),buf);
            sleep(9);
        }
        close(p1[1]);
        close(p2[0]);
        exit(0);
    }
    else
    {
        close(p1[1]);
        close(p2[0]);
        while(1)
        {
            if(read(p1[0],buf,5) != 5){
                fprintf(2, "father: read failed\n");
                break;
            }
            printf("father %d: received %s\n",getpid(),buf);
            sleep(1);
            if(write(p2[1],"pong",5) != 5){
                fprintf(2, "father: write failed\n");
                break;
            }
            sleep(9);
        }
    }
    wait(0);
    close(p1[0]);
    close(p2[1]);
    exit(0);
}