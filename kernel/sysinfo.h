struct sysinfo {
  uint64 freemem;   // amount of free memory (bytes)空闲内存的字节数
  uint64 nproc;     // number of process   state字段不为UNUSED的进程数
  uint64 memsize;   //平均负载
};
