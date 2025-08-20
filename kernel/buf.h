struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;    //设备号
  uint blockno; //块号
  struct sleeplock lock;
  uint refcnt;  //每个块的引用计数
  struct buf *prev; // LRU cache list 双向链表
  struct buf *next;
  uchar data[BSIZE];//数据
};

