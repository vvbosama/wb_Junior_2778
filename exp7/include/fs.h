// kernel/fs.h - 文件系统结构定义
#ifndef _FS_H_
#define _FS_H_

#include "types.h"
#include "proc.h"

// 文件系统常量
#define BSIZE           4096        // 块大小：4KB
#define BSIZE_SHIFT     12          // 块大小位移
#define NDIRECT         12          // 直接块数量
#define NINDIRECT       (BSIZE / sizeof(uint32_t))  // 间接块可索引的块数
#define MAXFILE         (NDIRECT + NINDIRECT)       // 最大文件块数
#define MAXOPBLOCKS     10          // 最大操作块数
#define LOGSIZE         (MAXOPBLOCKS * 3)  // 日志大小
#define FSSIZE          2000        // 文件系统大小（块数）

// 超级块位置
#define SUPERBLOCK_NUM  1           // 超级块在块1（块0是引导块）

// 文件系统魔数
#define FS_MAGIC        0x10203040

// 文件类型
#define T_DIR            1          // 目录
#define T_FILE           2          // 文件
#define T_DEVICE         3          // 设备

// 目录项大小
#define DIRSIZ           14          // 目录项中文件名最大长度

// 超级块结构
struct superblock {
    uint32_t magic;        // 文件系统魔数
    uint32_t size;         // 文件系统大小（块数）
    uint32_t nblocks;      // 数据块数量
    uint32_t ninodes;      // inode数量
    uint32_t nlog;         // 日志块数量
    uint32_t logstart;     // 日志起始块号
    uint32_t inodestart;   // inode区起始块号
    uint32_t bmapstart;    // 位图起始块号
};

// 磁盘inode结构
struct dinode {
    uint16_t type;         // 文件类型
    uint16_t major;        // 主设备号（T_DEVICE）
    uint16_t minor;        // 次设备号（T_DEVICE）
    uint16_t nlink;        // 硬链接计数
    uint32_t size;         // 文件大小（字节）
    uint32_t addrs[NDIRECT+1]; // 数据块地址
    uint64_t ctime;        // 创建时间戳
};

// 内存inode结构
struct inode {
    uint32_t dev;          // 设备号
    uint32_t inum;         // inode号
    int ref;               // 引用计数
    volatile int lock;     // 保护inode内容的锁
    int valid;             // inode已从磁盘读取？
    
    // 从磁盘拷贝的内容
    uint16_t type;
    uint16_t major;
    uint16_t minor;
    uint16_t nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT+1];
    uint64_t ctime;
};

// 目录项结构
struct dirent {
    uint16_t inum;         // inode号，0表示空闲
    char name[DIRSIZ];     // 文件名
};

// 文件结构
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;               // 引用计数
    char readable;
    char writable;
    struct inode *ip;      // FD_INODE和FD_DEVICE
    uint32_t off;          // FD_INODE
    short major;            // FD_DEVICE
};

// 统计信息结构（用于filestat）
struct stat {
    int dev;     // 设备号
    uint32_t ino; // inode号
    short type;   // 文件类型
    short nlink;  // 硬链接数
    uint64_t size; // 文件大小
};

#define NINODE 50  // 内存中缓存的inode数量
#define ROOTDEV 1  // 根设备号
#define NDEV 10    // 最大设备号

// 全局变量声明
extern struct superblock sb;  // 超级块

// 文件系统函数声明
void fsinit(int dev);
void readsb(int dev, struct superblock *sb);
struct inode* dirlookup(struct inode *dp, char *name, uint32_t *poff);
int dirlink(struct inode *dp, char *name, uint32_t inum);
struct inode* namei(char *path);
struct inode* nameiparent(char *path, char *name);
struct inode* iget(uint32_t dev, uint32_t inum);
void iput(struct inode *ip);
void iunlockput(struct inode *ip);
struct inode* ialloc(uint32_t dev, uint16_t type);
void iupdate(struct inode *ip);
void itrunc(struct inode *ip);
void stati(struct inode *ip, struct stat *st);
int readi(struct inode *ip, int user_dst, uint64_t dst, uint32_t off, uint32_t n);
int writei(struct inode *ip, int user_src, uint64_t src, uint32_t off, uint32_t n);
int namecmp(const char *s, const char *t);

// 文件操作函数
struct file* filealloc(void);
void fileclose(struct file *f);
struct file* filedup(struct file *f);
int fileread(struct file *f, uint64_t addr, int n);
int filewrite(struct file *f, uint64_t addr, int n);
int filestat(struct file *f, uint64_t addr);


extern void fs_reset_allocator(void);  // 添加重置函数声明

#endif // _FS_H_

