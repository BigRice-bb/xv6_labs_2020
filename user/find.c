#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *filename)
{
    int fd;
    struct stat st; // 存储文件信息的结构体
    char buf[512], *p;
    struct dirent de; // 文件目录项结构体
    // 打开文件  根据传入的path
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // 获取文件信息
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    // 如果 第一个参数不是目录文件  错误
    if (st.type != T_DIR)
    {
        fprintf(2, "find: %s is not a directory\n", path);
        close(fd);
        return;
    }
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        printf("ls: path too long\n");
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *(p++) = '/'; // p指向buf最后一个字符'/'的下一个位置
    while (read(fd, &de, sizeof(de)) == sizeof(de))//读出一个文件给de
    {
        if (de.inum == 0)//空
            continue;
        memmove(p, de.name, DIRSIZ);//加长路径
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)//获取文件信息
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        //如果buf是目录文件 递归查找
        if (st.type == T_DIR && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0)
        {
            find(buf, filename); // 递归查找
        }
        else if (strcmp(de.name, filename) == 0)
        {
            printf("%s\n", buf); // 找到文件
        }
    }
    close(fd); // 关闭文件
}
int main(int argc, char **argv)
{
    // 递归查找文件 argv[1]为文件名
    if (argc < 3)
    {
        printf("usage : find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]); // 递归查找argv[1]目录下的argv[2]文件
    exit(0);
}