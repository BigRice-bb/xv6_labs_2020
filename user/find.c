#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}
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
        else if (match(filename, de.name) != 0)
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
