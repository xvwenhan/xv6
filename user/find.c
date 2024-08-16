#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path)
{
  static char buf[DIRSIZ + 1];
  char *p;
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  // 这里不够长时没有填充空格，否则比较将不正确
  buf[strlen(p)] = '\0'; // 添加尾零，防止比较字符串时出问题
  return buf;
}

void find(char *dir, char *name)
{
  char buf[512], *p;
  struct dirent de; // 获取目录条目信息
  struct stat st;   // 获取文件类型
  int fd;
  int fd2;
  // 打开目标目录
  if ((fd = open(dir, 0)) < 0)
  {
    fprintf(2, "find: cannot open %s\n", dir);
    return;
  }
  // 读取目录中的内容
  strcpy(buf, dir);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de))
  {
    if (de.inum == 0)
      continue;
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
    {
      // 排除为.和..的情况
      continue;
    }
    memmove(p, de.name, DIRSIZ); // 构造路径
    p[DIRSIZ] = 0;               // ASCII为0的字符即尾零
    if ((fd2 = open(buf, 0)) < 0)
    {
      fprintf(2, "find: cannot open %s\n", buf);
      return;
    }
    if (fstat(fd2, &st) < 0)
    {
      fprintf(2, "find: cannot stat %s\n", buf);
      close(fd2);
      return;
    }
    if (st.type == T_DIR)
    {
      close(fd2);
      find(buf, name);
      continue;
    }
    else
    {
      if (strcmp(fmtname(buf), name) == 0)
      {
        printf("%s\n", buf);
      }
      close(fd2);
    }
  }
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(2, "Usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);

  exit(0);
}
