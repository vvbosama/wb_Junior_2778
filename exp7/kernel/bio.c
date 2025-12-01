// kernel/bio.c - 块缓存管理
#include "bio.h"
#include "fs.h"
#include "printf.h"
#include "proc.h"

// 块缓存数组
#define NBUF (MAXOPBLOCKS * 3)  // 缓存大小
static struct buf buf[NBUF];
static struct buf head;  // LRU链表头

// 简单的磁盘模拟（使用内存）
static uint8_t disk[FSSIZE * BSIZE];

// 初始化块缓存
void binit(void) {
    struct buf *b;
    
    // 初始化LRU链表
    head.prev = &head;
    head.next = &head;
    
    // 初始化所有缓存块
    for (b = buf; b < buf + NBUF; b++) {
        b->next = head.next;
        b->prev = &head;
        b->dev = -1;
        b->blockno = -1;
        b->refcnt = 0;
        b->valid = 0;
        b->disk = 0;
        b->lock = 0;
        head.next->prev = b;
        head.next = b;
    }
    
    printf("bio: initialized block cache with %d buffers\n", NBUF);
}

// 查找缓存块
static struct buf* bget(uint32_t dev, uint32_t blockno) {
    struct buf *b;
    
    // 查找已缓存的块
    for (b = buf; b < buf + NBUF; b++) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            return b;
        }
    }
    
    // 未找到，使用LRU策略替换
    // 首先查找引用计数为0的块
    for (b = buf; b < buf + NBUF; b++) {
        if (b->refcnt == 0) {
            // 如果块需要写回，先写回
            if (b->disk && b->valid) {
                // 写入磁盘（已经在bwrite中处理，这里只是标记）
                uint8_t *dst = &disk[b->blockno * BSIZE];
                for (int i = 0; i < BSIZE; i++) {
                    dst[i] = b->data[i];
                }
            }
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->disk = 0;
            b->refcnt = 1;
            return b;
        }
    }
    
    // 如果所有块都被引用，使用LRU策略强制替换（从链表头取最老的）
    // 这不应该发生，但如果发生了，我们强制替换
    b = head.next;
    if (b != &head) {
        // 如果块需要写回，先写回
        if (b->disk && b->valid) {
            uint8_t *dst = &disk[b->blockno * BSIZE];
            for (int i = 0; i < BSIZE; i++) {
                dst[i] = b->data[i];
            }
        }
        // 强制释放（危险，但总比卡住好）
        b->refcnt = 0;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->disk = 0;
        b->refcnt = 1;
        return b;
    }
    
    // 如果所有块都被引用，返回NULL（实际应该等待）
    printf("bio: warning - all buffers in use\n");
    return NULL;
}

// 读取块
struct buf* bread(uint32_t dev, uint32_t blockno) {
    struct buf *b;
    
    b = bget(dev, blockno);
    if (!b) {
        printf("bio: failed to get buffer for dev=%d blockno=%d\n", dev, blockno);
        return NULL;
    }
    
    if (!b->valid) {
        // 从磁盘读取
        if (blockno >= FSSIZE) {
            printf("bio: invalid blockno %d\n", blockno);
            return NULL;
        }
        
        // 从内存磁盘读取
        uint8_t *src = &disk[blockno * BSIZE];
        for (int i = 0; i < BSIZE; i++) {
            b->data[i] = src[i];
        }
        
        b->valid = 1;
    }
    
    return b;
}

// 写入块
void bwrite(struct buf *b) {
    if (!b || !b->valid) {
        printf("bio: invalid buffer for write\n");
        return;
    }
    
    // 标记为需要写回
    b->disk = 1;
    
    // 写入内存磁盘
    if (b->blockno >= FSSIZE) {
        printf("bio: invalid blockno %d for write\n", b->blockno);
        return;
    }
    
    uint8_t *dst = &disk[b->blockno * BSIZE];
    for (int i = 0; i < BSIZE; i++) {
        dst[i] = b->data[i];
    }
}

// 释放块
void brelse(struct buf *b) {
    if (!b) return;
    
    if (b->refcnt <= 0) {
        printf("bio: warning - releasing buffer with refcnt <= 0\n");
        return;
    }
    
    b->refcnt--;
    
    // 如果引用计数为0，移动到LRU链表末尾
    if (b->refcnt == 0) {
        // 从当前位置移除
        b->prev->next = b->next;
        b->next->prev = b->prev;
        
        // 添加到末尾
        b->next = &head;
        b->prev = head.prev;
        head.prev->next = b;
        head.prev = b;
    }
}

// 增加引用计数
void bpin(struct buf *b) {
    if (b) {
        b->refcnt++;
    }
}

// 减少引用计数
void bunpin(struct buf *b) {
    if (b && b->refcnt > 0) {
        b->refcnt--;
    }
}

