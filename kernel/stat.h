#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device  文件所在磁盘设备的Id  
  uint ino;    // Inode number   // 文件的索引节点号
  short type;  // Type of file  // 文件类型
  short nlink; // Number of links to file  //文件的硬链接数
  uint64 size; // Size of file in bytes  // 文件大小
};
