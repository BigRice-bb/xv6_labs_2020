struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;    //设备号
  uint blockno; //块号
  struct sleeplock lock;
  uint refcnt;  //每个块的引用计数
  //简化为单向链表
  //struct buf *prev; // LRU cache list 双向链表
  struct buf *next;
  uchar data[BSIZE];//数据

  //记录buf的使用时间戳
  uint lastuse;
};

