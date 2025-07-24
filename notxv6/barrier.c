#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;//锁
  pthread_cond_t barrier_cond;//条件变量
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;//数据结构用来判断条件是否满足

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;//当前已经到达的线程
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  //第一步加锁,保证bstate操作的原子性
  pthread_mutex_lock(&bstate.barrier_mutex);
  //第二步判断条件是否满足
  bstate.nthread++;//当前已经到达的线程数+1
  if (bstate.nthread == nthread) {
    bstate.nthread = 0;//重置
    bstate.round++;//轮数+1
    pthread_cond_broadcast(&bstate.barrier_cond);//唤醒所有线程
  } else {
    //若条件不满足,则使用条件变量阻塞,等待其他线程到达
    //条件变量会自动解锁,等待bstate.barrier_cond条件
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);//阻塞
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);//解锁
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
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
  nthread = atoi(argv[1]);
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
