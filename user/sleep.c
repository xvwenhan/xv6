//为实验一：sleep
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "Usage: sleep ticks\n");//无参数 打印错误信息
    exit(1);
  }
  int ticks=atoi(argv[1]);
  sleep(ticks);
  //fprintf(1, "%d\n",t);
  exit(0);

}