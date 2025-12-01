// kernel/log.c - 日志系统实现
#include "log.h"
#include "fs.h"
#include "bio.h"
#include "printf.h"
#include "proc.h"

struct log log;

// 简单的内存拷贝函数（在文件开头定义）
static void *memcpy_impl(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

// 自旋锁操作
void acquire(struct spinlock *lk) {
    while (__sync_lock_test_and_set(&lk->locked, 1)) {
        // 自旋等待
    }
}

void release(struct spinlock *lk) {
    __sync_lock_release(&lk->locked);
}

// 初始化日志系统
void initlog(int dev, struct superblock *sb) {
    if (sizeof(struct logheader) >= BSIZE) {
        printf("log: logheader too large\n");
        return;
    }
    
    log.dev = dev;
    log.start = sb->logstart;
    log.size = sb->nlog;
    log.lock.locked = 0;
    log.outstanding = 0;
    log.committing = 0;
    
    recover_from_log();
    
    printf("log: initialized log system (start=%d, size=%d)\n", log.start, log.size);
}

// 从日志恢复 - 修复版本
void recover_from_log(void) {
    readsb(log.dev, &sb);
    
    acquire(&log.lock);
    
    // 读取日志头（第一个日志块）
    struct buf *lbuf = bread(log.dev, log.start);
    if (lbuf) {
        struct logheader *lh = (struct logheader *)lbuf->data;
        
        if (lh->n > 0) {
            // 🚨 关键修复：有未提交的日志，应该丢弃而不是恢复！
            printf("log: found %d uncommitted blocks in log - DISCARDING (simulating crash rollback)\n", lh->n);
            
            // 只是清除日志头，不将修改应用到磁盘
            // 这模拟了崩溃时未提交事务的丢失
            lh->n = 0;
            bwrite(lbuf);
            
            printf("log: uncommitted transactions rolled back\n");
        } else {
            printf("log: no uncommitted transactions found\n");
        }
        
        brelse(lbuf);
    }
    
    // 重置日志状态
    log.lh.n = 0;
    log.outstanding = 0;
    log.committing = 0;
    
    release(&log.lock);
}

void begin_op(void) {
    acquire(&log.lock);
    
    // printf("LOG DEBUG: begin_op - before: outstanding=%d, committing=%d\n", 
    //        log.outstanding, log.committing);
    
    while (1) {
        if (log.committing) {
            // printf("LOG DEBUG: begin_op waiting for commit\n");
            release(&log.lock);
            continue;
        }
        if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
            // printf("LOG DEBUG: begin_op waiting for log space\n");
            release(&log.lock);
            continue;
        }
        log.outstanding += 1;
        release(&log.lock);
        break;
    }

    // printf("LOG DEBUG: begin_op - after: outstanding=%d\n", log.outstanding);
}

void end_op(void) {
    int do_commit = 0;
    
    acquire(&log.lock);
    
    // printf("LOG DEBUG: end_op - before: outstanding=%d, committing=%d, lh.n=%d\n",
    //        log.outstanding, log.committing, log.lh.n);
    
    log.outstanding -= 1;
    if (log.committing) {
        // printf("LOG ERROR: committing while outstanding operations\n");
        return;
    }
    if (log.outstanding == 0) {
        do_commit = 1;
        log.committing = 1;
        // printf("LOG DEBUG: Triggering commit, lh.n=%d\n", log.lh.n);
    } else {
        // printf("LOG DEBUG: Not committing yet, outstanding=%d\n", log.outstanding);
    }
    release(&log.lock);
    
    if (do_commit) {
        // printf("LOG DEBUG: Starting commit process\n");
        commit();
        acquire(&log.lock);
        log.committing = 0;
        release(&log.lock);
        // printf("LOG DEBUG: Commit complete\n");
    }
}

// 提交事务
void commit(void) {
    if (log.lh.n > 0) {
        // 写入日志头
        struct buf *buf = bread(log.dev, log.start);
        struct logheader *hb = (struct logheader *)buf->data;
        hb->n = log.lh.n;
        for (int i = 0; i < log.lh.n; i++) {
            hb->block[i] = log.lh.block[i];
        }
        bwrite(buf);
        brelse(buf);
        
        // 写入日志数据
        for (int tail = 0; tail < log.lh.n; tail++) {
            struct buf *to = bread(log.dev, log.start + tail + 1);
            struct buf *from = bread(log.dev, log.lh.block[tail]);
            memcpy_impl(to->data, from->data, BSIZE);
            bwrite(to);
            brelse(from);
            brelse(to);
        }
        
        // 提交到磁盘
        if (log.lh.n > 0) {
            // 写入提交记录（简化：将日志头n设为0表示已提交）
            struct buf *buf = bread(log.dev, log.start);
            struct logheader *hb = (struct logheader *)buf->data;
            hb->n = 0;
            bwrite(buf);
            brelse(buf);
            
            // 将日志块写入实际位置
            for (int i = 0; i < log.lh.n; i++) {
                struct buf *to = bread(log.dev, log.lh.block[i]);
                struct buf *from = bread(log.dev, log.start + i + 1);
                memcpy_impl(to->data, from->data, BSIZE);
                bwrite(to);
                brelse(from);
                brelse(to);
            }
        }
        
        log.lh.n = 0;
    }
}

// 记录写操作到日志
void log_write(struct buf *b) {
    acquire(&log.lock);
    
    // 检查块是否已在日志中
    int i;
    for (i = 0; i < log.lh.n; i++) {
        if (log.lh.block[i] == b->blockno) {
            // 块已在日志中，只需更新标记
            b->disk = 1;
            release(&log.lock);
            return;
        }
    }
    
    // 检查日志空间
    if (log.lh.n >= LOGSIZE - MAXOPBLOCKS) {
        printf("log: maximum transaction size exceeded (n=%d, limit=%d)\n", 
               log.lh.n, LOGSIZE - MAXOPBLOCKS);
        release(&log.lock);
        return;
    }
    
    // 添加新块到日志
    log.lh.block[log.lh.n] = b->blockno;
    log.lh.n++;
    b->disk = 1;  // 标记为已记录到日志
    release(&log.lock);
}

