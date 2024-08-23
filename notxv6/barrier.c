#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;//记录线程总数
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;//互斥锁
  pthread_cond_t barrier_cond;//条件变量，用于线程间的同步
  int nthread;      //当前到达屏障的线程数量
  int round;     //屏障的轮次
} bstate;

static void
barrier_init(void)
{
  //初始化锁和条件变量
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;//设置 nthread 为 0，表示目前还没有线程到达屏障
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  
  if (++bstate.nthread < nthread) {
    //所有线程还没有到齐
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    //线程会释放互斥锁,然后进入等待状态(以便其他线程能够获得这个锁)
    //其他线程通过pthread_cond_broadcast()唤醒，该线程会重新获取 bstate.barrier_mutex 锁
  } else {
    bstate.round++;//完成一轮
    bstate.nthread = 0;//到齐清零
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);//检查当前线程的轮次是否等于当前屏障的轮
    barrier();
    usleep(random() % 100);//模拟随机延迟，使线程之间的时间差随机化
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);//创建 nthread 个线程，每个线程调用 thread 函数
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
