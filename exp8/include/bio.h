// kernel/bio.h - 块缓存系统
#ifndef _BIO_H_
#define _BIO_H_

#include "types.h"
#include "fs.h"

// 块缓存结构
struct buf {
    int valid;          // 缓存是否有效
    int disk;           // 是否需要写回磁盘
    uint32_t dev;       // 设备号
    uint32_t blockno;   // 块号
    volatile int lock;  // 保护缓存内容的锁
    uint32_t refcnt;    // 引用计数
    struct buf *prev;   // LRU链表前驱
    struct buf *next;   // LRU链表后继
    uint8_t data[BSIZE]; // 实际数据
};

// 块缓存函数
void binit(void);
struct buf* bread(uint32_t dev, uint32_t blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);

#endif // _BIO_H_

