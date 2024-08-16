#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//素数筛们的行动
void prime_sieve(int p[2])
{
    int prime, n;
    int next_p[2];
    //关闭写端（不需要）
    close(p[1]);
    // 读出通道中第一个数字，一定是素数
    if(read(p[0], &prime, sizeof(prime)) == 0)
        exit(0);//若读完所有数，退出该进程
    printf("prime %d\n", prime);
    // 创建一个新管道 和下一个素数筛沟通
    pipe(next_p);
    if(fork() == 0) {
        //创建子进程，即下一个素数筛
        close(p[0]); //子进程关闭当前管道的读端
        prime_sieve(next_p);//子进程重复素数筛的行为逻辑
    } else {
        close(next_p[0]); //关闭下一个管道的读端
        while(read(p[0], &n, sizeof(n)) != 0) {
            if(n % prime != 0) {
                write(next_p[1], &n, sizeof(n));//把剩余自己没接收的所有数字都传递给下一个管道
            }
        }
        //读完写完，关闭上一个管道的读端和下一个管道的写端
        close(p[0]); 
        close(next_p[1]);
        //等待子进程结束
        wait(0);
    }

    exit(0);
}

int main(int argc, char *argv[])
{

    if (argc != 1)
    {
        printf("Usage:pingpong");
        exit(1);
    }
    int p[2];
    pipe(p);

    if(fork() == 0) {
        prime_sieve(p);
        //主进程创建子进程，即第一个素数筛
    } else {
        close(p[0]); //关闭读端
        //主进程把所有数字写入第一个管道
        for(int i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]); //关闭写端
        wait(0); //等待子进程执行完毕
    }

    exit(0);
}
