#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};
//5个桶,每个桶是一个链表--链表节点是entry
struct entry *table[NBUCKET];
//100000个key
int keys[NKEYS];
//线程数
int nthread = 1;

//锁
//为每个桶创建一个锁,加快效率
pthread_mutex_t locks[NBUCKET];

double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  //堆区分配节点内存
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;//头插
  *p = e;
}



static 
void put(int key, int value)
{
  int i = key % NBUCKET;//哈希函数--找到桶号

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {//遍历桶链表 找到key是否存在
    if (e->key == key)//找到key，更新value
      break;
  }
  pthread_mutex_lock(&locks[i]);//加锁
  if(e){//如果key存在，更新value
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.

    //这步操作需要保证原子
    //1. 加锁
    //2. 插入
    //3. 解锁
    insert(key, value, &table[i], table[i]);//头插
  }
  pthread_mutex_unlock(&locks[i]);//解锁
}

//返回对应节点
static struct entry*
get(int key)
{
  int i = key % NBUCKET;//找到桶号


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;//如果key存在返回对应节点 ,不存在返回NULL
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;//b是每个线程要处理的key数量

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);
    //线程1 0-b-1
    //线程2 b-2b-1 ...
    //keys[]数组会被随机数填满
    //一共5个桶,key的值被分为5类 分到每个桶中
    //每个桶以链表的方式存储key-value(处理的线程号)
    //每个线程处理b个key 其中的一个key 其中的n就是线程号
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);//获取key对应的节点
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

void mutex_init()
{
  for (int i = 0; i < NBUCKET; i++)
  {
    pthread_mutex_init(&locks[i], NULL);
  }
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;
  mutex_init();

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);//数据类型转换
  tha = malloc(sizeof(pthread_t) * nthread);//分配线程句柄数组
  srandom(0);//固定随机种子
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();//随机生成100000个key
  }

  //
  // first the puts
  //
  t0 = now();//开始时间
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);//等待线程结束
  }
  t1 = now();//结束时间

  printf("%d puts, %.3f seconds, %.0f puts/second\n",//打印结果
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
