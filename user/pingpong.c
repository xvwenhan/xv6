#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[])
{

    if (argc != 1)
    {
        printf("Usage:pingpong");
        exit(1);
    }
    int p[2];
    int c[2];
    pipe(p);//父说子听 
    pipe(c);//子说父听
    int pid = fork();
    if (pid!=0){
        write(p[1],"ping\n",5);
        close(p[1]);
        char context[5];
        read(c[0], context, 5);
        int id = getpid();
        printf("%d: received %s\n", id, context);
    }
    else{
        char context[5];
        read(p[0], context, 5);
        int id = getpid();
        printf("%d: received %s\n", id, context);
        write(c[1],"pong\n",5);
        close(c[1]);
    }
    exit(0);
}
