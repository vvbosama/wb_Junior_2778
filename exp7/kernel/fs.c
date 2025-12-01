// kernel/fs.c - 文件系统核心实现
#include "fs.h"
#include "bio.h"
#include "log.h"
#include "printf.h"
#include "proc.h"

uint32_t next_free_block = 0;

struct superblock sb;
struct {
    struct inode inode[NINODE];
} icache;

// 简单的字符串函数
static int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        if (s1[i] == '\0') break;
    }
    return 0;
}

static void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

static void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

#define IPB (BSIZE / sizeof(struct dinode))

// 创建文件系统（前向声明）
static void mkfs(int dev) __attribute__((used));

// 初始化文件系统
void fsinit(int dev) {
    readsb(dev, &sb);
    if (sb.magic != FS_MAGIC) {
        // 如果文件系统不存在，创建新的
        printf("fs: filesystem not found, creating new filesystem...\n");
        mkfs(dev);
        readsb(dev, &sb);
    }
    
    if (sb.magic != FS_MAGIC) {
        printf("fs: invalid filesystem magic: %x\n", sb.magic);
        return;
    }
    
    initlog(dev, &sb);
    printf("fs: filesystem initialized (size=%d blocks, ninodes=%d)\n", 
           sb.size, sb.ninodes);
}

// 创建文件系统
static void mkfs(int dev) {
    struct buf *bp;
    struct superblock sb_new;
    
    // 初始化超级块
    sb_new.magic = FS_MAGIC;
    sb_new.size = FSSIZE;
    sb_new.nblocks = FSSIZE - 1;  // 减去超级块
    sb_new.ninodes = 200;
    sb_new.nlog = LOGSIZE;
    sb_new.logstart = 2;
    sb_new.inodestart = sb_new.logstart + sb_new.nlog;
    sb_new.bmapstart = sb_new.inodestart + (sb_new.ninodes / IPB) + 1;
    
    // 写入超级块
    bp = bread(dev, SUPERBLOCK_NUM);
    memcpy(bp->data, &sb_new, sizeof(sb_new));
    bwrite(bp);
    brelse(bp);
    
    // 更新全局超级块
    memcpy(&sb, &sb_new, sizeof(sb));
    
    // 初始化根目录inode（手动分配inode 1）
    // inode 1 在第一个 inode 块中，索引为 1 % IPB
    bp = bread(dev, sb.inodestart + 1 / IPB);
    if (bp) {
        struct dinode *dip = (struct dinode *)bp->data;
        // 清零整个 inode 结构
        memset(&dip[1 % IPB], 0, sizeof(struct dinode));
        // 初始化根目录
        dip[1 % IPB].type = T_DIR;
        dip[1 % IPB].nlink = 1;
        dip[1 % IPB].size = 0;
        dip[1 % IPB].ctime = 0;
        // 确保地址数组被清零
        for (int i = 0; i < NDIRECT + 1; i++) {
            dip[1 % IPB].addrs[i] = 0;
        }
        bwrite(bp);
        brelse(bp);
    }
    
    // 重置块分配器
    next_free_block = 0;
    printf("fs: created new filesystem\n");
}

// 读取超级块
void readsb(int dev, struct superblock *sb) {
    struct buf *bp;
    
    bp = bread(dev, SUPERBLOCK_NUM);
    memcpy(sb, bp->data, sizeof(*sb));
    brelse(bp);
}

#define IPB (BSIZE / sizeof(struct dinode))

// 从磁盘读取inode
static void iread(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;
    
    if (ip->valid == 0) {
        bp = bread(ip->dev, sb.inodestart + ip->inum / IPB);
        if (!bp) {
            return;
        }
        dip = (struct dinode *)bp->data + (ip->inum % IPB);
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        ip->ctime = dip->ctime;
        // 确保 addrs 数组被正确初始化，并验证每个地址
        for (int i = 0; i < NDIRECT + 1; i++) {
            ip->addrs[i] = dip->addrs[i];
            // 验证地址有效性：如果地址不为0，必须在有效范围内
            if (ip->addrs[i] != 0 && (ip->addrs[i] >= sb.size || ip->addrs[i] < sb.bmapstart)) {
                // 无效地址，清零
                ip->addrs[i] = 0;
            }
        }
        brelse(bp);
        ip->valid = 1;
    }
}

// 获取inode（公共接口）
struct inode* iget(uint32_t dev, uint32_t inum) {
    struct inode *ip;
    
    // 简化实现：直接查找或分配
    for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            return ip;
        }
    }
    
    // 分配新的 - 添加边界检查
    for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
        if (ip->ref == 0) {
            // 检查是否越界访问了 next_free_block
            if ((uint64_t)ip >= (uint64_t)&next_free_block && 
                (uint64_t)ip < (uint64_t)&next_free_block + sizeof(next_free_block)) {
                printf("ERROR: inode cache overlaps with next_free_block!\n");
                printf("  ip=%p, next_free_block=%p\n", ip, &next_free_block);
                return 0;
            }
            
            // 初始化 inode 结构
            ip->dev = dev;
            ip->inum = inum;
            ip->ref = 1;
            ip->valid = 0;
            ip->type = 0;
            ip->size = 0;
            ip->nlink = 0;
            ip->major = 0;
            ip->minor = 0;
            // 初始化地址数组
            for (int i = 0; i < NDIRECT + 1; i++) {
                ip->addrs[i] = 0;
            }
            ip->ctime = 0;
            iread(ip);
            return ip;
        }
    }
    
    return 0;
}

// 释放inode
void iput(struct inode *ip) {
    if (ip == 0 || ip->ref < 1) {
        return;
    }
    
    ip->ref--;
    if (ip->ref == 0 && ip->nlink == 0) {
        // inode未使用，可以释放
        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;
    }
}

// 解锁并释放inode
void iunlockput(struct inode *ip) {
    iput(ip);
}

// 分配inode
struct inode* ialloc(uint32_t dev, uint16_t type) {
    int inum;
    struct buf *bp;
    struct dinode *dip;
    
    for (inum = 1; inum < sb.ninodes; inum++) {
        uint32_t blockno = sb.inodestart + inum / IPB;
        bp = bread(dev, blockno);
        if (!bp) {
            continue;
        }
        dip = (struct dinode *)bp->data + (inum % IPB);
        if (dip->type == 0) {
            // 找到空闲inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            dip->ctime = 0;
            log_write(bp);  // 只有修改的块才记录到日志
            brelse(bp);
            struct inode *ip = iget(dev, inum);
            return ip;
        }
        brelse(bp);  // 只读的块不需要记录到日志
    }
    
    printf("fs: ialloc - no inodes\n");
    return 0;
}

// 更新inode到磁盘
void iupdate(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;
    
    bp = bread(ip->dev, sb.inodestart + ip->inum / IPB);
    dip = (struct dinode *)bp->data + (ip->inum % IPB);
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
    dip->ctime = ip->ctime;
    log_write(bp);
    brelse(bp);
}

// 截断文件
void itrunc(struct inode *ip) {
    int i, j;
    struct buf *bp;
    uint32_t *a;
    
    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            // 释放直接块（简化实现）
            ip->addrs[i] = 0;
        }
    }
    
    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint32_t *)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j]) {
                // 释放间接块（简化实现）
                a[j] = 0;
            }
        }
        brelse(bp);
        // 释放间接块本身（简化实现）
        ip->addrs[NDIRECT] = 0;
    }
    
    ip->size = 0;
    iupdate(ip);
}


// 块映射：将文件内的逻辑块号转换为物理块号
static uint32_t bmap(struct inode *ip, uint32_t bn) {
    uint32_t addr = 0, *a;
    struct buf *bp;

    // printf("DEBUG bmap: inode=%d, bn=%d, next_free_block=%d\n", 
    //     ip->inum, bn, next_free_block);
        
     // 内存保护：检查 next_free_block 是否被破坏
    if (next_free_block > 1000000) {
        printf("EMERGENCY: next_free_block corrupted to %d, resetting to 5\n", next_free_block);
        next_free_block = 5;  // 从上次正常值继续
    }
    
    // printf("DEBUG bmap: inode=%d, bn=%d, next_free_block=%d\n", 
    //     ip->inum, bn, next_free_block);


    if (bn < NDIRECT) {
        // 检查并验证已有的块号
        if (ip->addrs[bn] != 0) {
            addr = ip->addrs[bn];
            // 验证块号有效性
            if (addr >= sb.size || addr < sb.bmapstart) {
                // 无效块号，视为未分配
                printf("fs: bmap - invalid block number %d (bmapstart=%d, size=%d), resetting\n", 
                       addr, sb.bmapstart, sb.size);
                ip->addrs[bn] = 0;
                // addr = 0;  // 重置 addr
            } else {
                return addr;
            }
        }
        
        // 分配新块（只有在 addrs[bn] == 0 时才分配）
        if (ip->addrs[bn] == 0) {
            uint32_t data_start = sb.bmapstart;

            // 确保不会超出文件系统范围
        if (next_free_block >= (sb.size - data_start)) {
            printf("fs: bmap - out of disk space (no free blocks)\n");
            return 0;
        }
            addr = data_start + next_free_block;
            next_free_block++;
            
            // 检查是否超出文件系统大小
            if (addr >= sb.size) {
                printf("fs: bmap - out of disk space (block %d >= size %d, bmapstart=%d, next=%d)\n", 
                       addr, sb.size, sb.bmapstart, next_free_block - 1);
                next_free_block--;  // 回退
                return 0;
            }
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;
    
    if (bn < NINDIRECT) {
        printf("DEBUG: handling indirect block, bn=%d\n", bn);
        if (ip->addrs[NDIRECT] == 0) {
            printf("DEBUG: allocating indirect block\n");
            // 分配间接块
            uint32_t data_start = sb.bmapstart;
            addr = data_start + next_free_block;
            next_free_block++;
            if (addr >= sb.size) {
                printf("fs: bmap - out of disk space for indirect block\n");
                next_free_block--;  // 回退
                return 0;
            }
            ip->addrs[NDIRECT] = addr;
            // 初始化间接块（清零）
            bp = bread(ip->dev, addr);
            if (!bp) {
                return 0;
            }
            memset(bp->data, 0, BSIZE);
            log_write(bp);
            brelse(bp);
        } else {
            addr = ip->addrs[NDIRECT];
            if (addr >= sb.size || addr < sb.bmapstart) {
                printf("fs: bmap - invalid indirect block number %d\n", addr);
                return 0;
            }
        }
        bp = bread(ip->dev, addr);
        if (!bp) {
            return 0;
        }
        a = (uint32_t *)bp->data;
        // 检查间接块是否已初始化（如果包含无效值，视为未初始化）
        if (a[bn] == 0 || a[bn] >= sb.size || a[bn] < sb.bmapstart) {
            // 分配数据块
            uint32_t data_start = sb.bmapstart;
            uint32_t new_addr = data_start + next_free_block;
            next_free_block++;
            if (new_addr >= sb.size) {
                printf("fs: bmap - out of disk space for data block\n");
                next_free_block--;  // 回退
                brelse(bp);
                return 0;
            }
            a[bn] = new_addr;
            log_write(bp);
            addr = new_addr;
        } else {
            addr = a[bn];
            // 再次验证块号有效性
            if (addr >= sb.size || addr < sb.bmapstart) {
                printf("fs: bmap - invalid indirect block entry %d\n", addr);
                brelse(bp);
                return 0;
            }
        }
        brelse(bp);
        return addr;
    }
    
    printf("fs: bmap - out of range (bn=%d)\n", bn + NDIRECT);
    return 0;
}

// 读取inode数据
int readi(struct inode *ip, int user_dst, uint64_t dst, uint32_t off, uint32_t n) {
    uint32_t tot, m;
    struct buf *bp;
    
    if (off > ip->size || off + n < off) {
        return 0;
    }
    if (off + n > ip->size) {
        n = ip->size - off;
    }
    
    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        bp = bread(ip->dev, bmap(ip, off / BSIZE));
        m = BSIZE - (off % BSIZE);
        if (m > n - tot) {
            m = n - tot;
        }
        
        if (user_dst) {
            // 拷贝到用户空间（简化实现）
            memcpy((void *)dst, (char *)bp->data + (off % BSIZE), m);
        } else {
            memcpy((void *)dst, (char *)bp->data + (off % BSIZE), m);
        }
        brelse(bp);
    }
    return n;
}

// 写入inode数据
int writei(struct inode *ip, int user_src, uint64_t src, uint32_t off, uint32_t n) {
    uint32_t tot, m;
    struct buf *bp;
    
    if (off > ip->size || off + n < off) {
        return -1;
    }
    if (off + n > MAXFILE * BSIZE) {
        return -1;
    }
    
    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        bp = bread(ip->dev, bmap(ip, off / BSIZE));
        m = BSIZE - (off % BSIZE);
        if (m > n - tot) {
            m = n - tot;
        }
        
        if (user_src) {
            // 从用户空间拷贝（简化实现）
            memcpy((char *)bp->data + (off % BSIZE), (void *)src, m);
        } else {
            memcpy((char *)bp->data + (off % BSIZE), (void *)src, m);
        }
        log_write(bp);
        brelse(bp);
    }
    
    if (off > ip->size) {
        ip->size = off;
    }
    iupdate(ip);
    
    return n;
}

// 目录查找
struct inode* dirlookup(struct inode *dp, char *name, uint32_t *poff) {
    uint32_t off, inum;
    struct dirent de;
    
    if (dp->type != T_DIR) {
        printf("fs: dirlookup not DIR\n");
        return 0;
    }
    
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
            return 0;
        }
        if (de.inum == 0) {
            continue;
        }
        if (namecmp(name, de.name) == 0) {
            if (poff) {
                *poff = off;
            }
            inum = de.inum;
            return iget(dp->dev, inum);
        }
    }
    
    return 0;
}

// 目录链接
int dirlink(struct inode *dp, char *name, uint32_t inum) {
    int off;
    struct dirent de;
    struct inode *ip;
    
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }
    
    // 查找空闲目录项
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
            return -1;
        }
        if (de.inum == 0) {
            break;
        }
    }
    
    // 拷贝文件名
    int len = strlen(name);
    if (len >= DIRSIZ) len = DIRSIZ - 1;
    memcpy(de.name, name, len);
    de.name[len] = '\0';
    de.inum = inum;
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        return -1;
    }
    
    return 0;
}

// 字符串比较
int namecmp(const char *s, const char *t) {
    return strncmp(s, t, DIRSIZ);
}

// 字符串拷贝（当前未使用）
// static void strncpy(char *dst, const char *src, int n) {
//     int i;
//     for (i = 0; i < n && src[i]; i++) {
//         dst[i] = src[i];
//     }
//     for (; i < n; i++) {
//         dst[i] = '\0';
//     }
// }

// 路径解析辅助函数
static char* skipelem(char *path, char *name) {
    char *s;
    int len;
    
    while (*path == '/') {
        path++;
    }
    if (*path == 0) {
        return 0;
    }
    s = path;
    while (*path != '/' && *path != 0) {
        path++;
    }
    len = path - s;
    if (len >= DIRSIZ) {
        memcpy(name, s, DIRSIZ);
    } else {
        memcpy(name, s, len);
        name[len] = 0;
    }
    while (*path == '/') {
        path++;
    }
    return path;
}

// 路径查找
static struct inode* namex(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;
    
    if (*path == '/') {
        // 从根目录开始（简化：使用inode 1作为根）
        ip = iget(ROOTDEV, 1);
    } else {
        // 从当前目录开始（简化：使用根目录）
        ip = iget(ROOTDEV, 1);
    }
    
    while ((path = skipelem(path, name)) != 0) {
        if (ip->type != T_DIR) {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

// 查找路径对应的inode
struct inode* namei(char *path) {
    char name[DIRSIZ];
    return namex(path, 0, name);
}

// 查找父目录
struct inode* nameiparent(char *path, char *name) {
    return namex(path, 1, name);
}

// 获取inode统计信息
void stati(struct inode *ip, struct stat *st) {
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

void fs_reset_allocator(void) {
    printf("DEBUG: before reset, next_free_block address=%p, value=%d\n", 
           &next_free_block, next_free_block);
    
    next_free_block = 0;
    
    printf("DEBUG: after reset, next_free_block=%d\n", next_free_block);
    
    // 验证写入是否成功
    if (next_free_block != 0) {
        printf("ERROR: failed to reset next_free_block! Still %d\n", next_free_block);
        // 强制修复
        volatile uint32_t *ptr = &next_free_block;
        *ptr = 0;
        printf("DEBUG: forced reset to %d\n", next_free_block);
    }
}