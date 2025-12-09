// kernel/file.c - 文件描述符管理
#include "fs.h"
#include "log.h"
#include "printf.h"
#include "proc.h"
#include "syscall.h"

#define NFILE 100  // 最大打开文件数

struct {
    struct file file[NFILE];
} ftable;

// 分配文件结构
struct file* filealloc(void) {
    struct file *f;
    
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            return f;
        }
    }
    return 0;
}

// 关闭文件
void fileclose(struct file *f) {
    if (f->ref < 1) {
        printf("file: close - bad ref count\n");
        return;
    }
    
    if (--f->ref > 0) {
        return;
    }
    
    if (f->type == FD_INODE || f->type == FD_DEVICE) {
        iput(f->ip);
    }
    
    f->type = FD_NONE;
    f->ref = 0;
    f->ip = 0;
}

// 复制文件描述符
struct file* filedup(struct file *f) {
    if (f->ref < 1) {
        printf("file: dup - bad ref count\n");
        return 0;
    }
    f->ref++;
    return f;
}

// 读取文件
int fileread(struct file *f, uint64_t addr, int n) {
    int r = 0;
    
    if (f->readable == 0) {
        return -1;
    }
    
    if (f->type == FD_INODE) {
        r = readi(f->ip, 1, addr, f->off, n);
        if (r > 0) {
            f->off += r;
        }
    } else {
        printf("file: read - unsupported file type\n");
        return -1;
    }
    
    return r;
}

// 写入文件
int filewrite(struct file *f, uint64_t addr, int n) {
    int r = 0;
    int i = 0;
    
    if (f->writable == 0) {
        return -1;
    }
    
    if (f->type == FD_INODE) {
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
        i = 0;
        while (i < n) {
            int n1 = n - i;
            if (n1 > max) {
                n1 = max;
            }
            begin_op();
            r = writei(f->ip, 1, addr + i, f->off, n1);
            end_op();
            if (r < 0) {
                break;
            }
            if (r != n1) {
                printf("file: write - short write\n");
            }
            i += r;
            f->off += r;
        }
    } else {
        printf("file: write - unsupported file type\n");
        return -1;
    }
    
    return (i == n) ? n : -1;
}

// 获取文件统计信息
int filestat(struct file *f, uint64_t addr) {
    struct stat st;
    struct proc *p = curr_proc;
    
    if (f->type == FD_INODE || f->type == FD_DEVICE) {
        stati(f->ip, &st);
        // 拷贝到用户空间（简化实现）
        if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0) {
            return -1;
        }
        return 0;
    }
    return -1;
}

