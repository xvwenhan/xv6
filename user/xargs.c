#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    char buf[512];
    char *p;
    int n;
    
    char *args[MAXARG];
    int i;
    
    // 将原始命令存储在 args 中
    for(i = 0; i < argc - 1; i++) {
        args[i] = argv[i + 1];
    }

    while ((n = read(0, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = 0;  // 确保字符串以 '\0' 结尾
        p = buf;
        while (*p) {
            char *start = p;
            while (*p && *p != '\n') p++;
            *p = 0;  // 将 '\n' 替换为 '\0' 以结束字符串·
            args[i] = start;  // 将读入的行追加为命令的最后一个参数
            args[i + 1] = 0;  // 以 NULL 结束参数列表
            
            if (fork() == 0) {
                exec(argv[1], args);  // 子进程执行命令
                exit(0);
            } else {
                wait(0);  // 父进程等待子进程
            }
            p++;
        }
    }

    exit(0);
}
