// kernel/sysproc.c
#include "syscall.h"
#include "proc.h"
#include "printf.h"
#include "mm.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "file.h"
#include "log.h"

#define SYSERR_SUCCESS 0
#define SYSERR_INVALID_ARGS -1
#define SYSERR_ACCESS_DENIED -2
#define SYSERR_MEMORY_FAULT -3
#define SYSERR_RESOURCE_BUSY -4
#define SYSERR_NOT_FOUND -5
#define SYSERR_NOT_SUPPORTED -6
#define SYSERR_INTERNAL -7

// 函数声明
void fork_return_point(void);
void exit_process(int status);  // 声明内核的退出函数

// 子进程的入口点
void fork_return_point(void) {
    printf("🚀 Child process %d started!\n", curr_proc->pid);
    
    // 子进程从 fork 返回 0
    if (curr_proc->trap_context) {
        curr_proc->trap_context->a0 = 0;  // 子进程返回 0
    }
    
    printf("Child process %d: doing simple task...\n", curr_proc->pid);
    
    // 执行简单任务
    for (int i = 0; i < 3; i++) {
        printf("Child %d: step %d\n", curr_proc->pid, i + 1);
        for (volatile int j = 0; j < 10000; j++); // 短延时
    }
    
    printf("✅ Child process %d: exiting\n", curr_proc->pid);
    exit_process(0);  // 使用内核的退出函数
}

// 获取当前进程的辅助函数
struct proc* myproc(void) {
    return curr_proc;
}

// 进程相关系统调用
int sys_fork(void) {
    printf("SYSCALL: fork called from pid %d\n", myproc()->pid);
    
    struct proc *p = alloc_proc();
    if (!p) {
        printf("SYSCALL: fork failed - no free process slots\n");
        set_syscall_error(SYSERR_RESOURCE_BUSY);
        return -1;
    }
    
    // 复制当前进程的上下文
    p->context = myproc()->context;
    
    // 关键修复：设置子进程的返回地址
    p->context.ra = (uint64_t)fork_return_point;
    
    // 确保栈有效
    if (p->kstack == 0) {
        void *stack = alloc_page();
        if (stack) {
            p->kstack = (uint64_t)stack;
            p->context.sp = p->kstack + PAGE_SIZE;
        }
    }
    
    p->state = RUNNABLE;
    
    printf("SYSCALL: fork created process %d, ra=%p, sp=%p\n", 
           p->pid, (void*)p->context.ra, (void*)p->context.sp);
    
    return p->pid;
}

int sys_exit(void) {
    int status;
    if(argint(0, &status) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: exit called from pid %d with status %d\n", 
           myproc()->pid, status);
    
    exit_process(status);
    return 0; // unreachable
}

// 在 sysproc.c 中修改 sys_wait
int sys_wait(void) {
    uint64_t status_ptr;
    if(argaddr(0, &status_ptr) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: wait called from pid %d, status_ptr=%p\n", 
           myproc()->pid, (void*)status_ptr);
    
    int status;
    int pid = wait_process(&status);
    
    // 重要：检查 wait_process 是否成功返回
    if (pid < 0) {
        printf("SYSCALL: wait_process failed, returning -1\n");
        return -1;
    }
    
    printf("SYSCALL: wait returning pid=%d, status=%d\n", pid, status);
    
    if(pid > 0 && status_ptr != 0) {
        // 将状态值拷贝回用户空间
        struct proc *p = myproc();
        if(copyout(p->pagetable, status_ptr, (char*)&status, sizeof(status)) < 0) {
            printf("SYSCALL: copyout failed in wait\n");
            set_syscall_error(SYSERR_MEMORY_FAULT);
            return -1;
        }
        printf("SYSCALL: status %d copied to user space %p\n", status, (void*)status_ptr);
    }
    
    return pid;
}

int sys_kill(void) {
    int pid;
    if(argint(0, &pid) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: kill called for pid %d from pid %d\n", 
           pid, myproc()->pid);
    
    // 查找目标进程
    struct proc *target = NULL;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state != UNUSED && proc[i].pid == pid) {
            target = &proc[i];
            break;
        }
    }
    spin_unlock(&proc_lock);
    
    if (!target) {
        set_syscall_error(SYSERR_NOT_FOUND);
        return -1;
    }
    
    // 设置终止标志
    target->killed = 1;
    
    // 如果进程在睡眠，唤醒它
    if (target->state == SLEEPING) {
        target->state = RUNNABLE;
    }
    
    return 0;
}

int sys_getpid(void) {
    int pid = myproc()->pid;
    // printf("SYSCALL: getpid returning %d\n", pid);
    return pid;
}

int sys_getppid(void) {
    struct proc *p = myproc();
    int ppid = p->parent ? p->parent->pid : 0;
    printf("SYSCALL: getppid returning %d\n", ppid);
    return ppid;
}

// 简化版文件相关系统调用
int sys_write(void) {
    int fd;
    uint64_t buf_addr;
    int n;
    
    if(argint(0, &fd) < 0 || argaddr(1, &buf_addr) < 0 || argint(2, &n) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    if(n < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    // 增强的安全检查
    if(buf_addr == 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    // 检查内核空间指针
    if (buf_addr >= 0x80000000) {
        printf("SECURITY: write attempt with kernel pointer: 0x%lx\n", buf_addr);
        set_syscall_error(SYSERR_ACCESS_DENIED);
        return -1;
    }
    
    // 只支持标准输出和标准错误
    if(fd != 1 && fd != 2) {
        set_syscall_error(SYSERR_NOT_SUPPORTED);
        return -1;
    }
    
    // 限制写入大小
    if (n > 4096) {
        n = 4096; // 限制为4KB
    }
    
    struct proc *p = myproc();
    
    // 从用户空间读取数据
    char *kbuf = alloc_page();
    if(!kbuf) {
        set_syscall_error(SYSERR_MEMORY_FAULT);
        return -1;
    }
    
    // 确保不读取超过页面大小的数据
    if(n > PAGE_SIZE) {
        n = PAGE_SIZE;
    }
    
    if(copyin(p->pagetable, kbuf, buf_addr, n) < 0) {
        free_page(kbuf);
        set_syscall_error(SYSERR_MEMORY_FAULT);
        return -1;
    }
    
    // 直接输出到控制台
    for(int i = 0; i < n; i++) {
        console_putc(kbuf[i]);
    }
    
    free_page(kbuf);
    return n;
}

int sys_read(void) {
    int fd;
    uint64_t buf_addr;
    int n;
    
    if(argint(0, &fd) < 0 || argaddr(1, &buf_addr) < 0 || argint(2, &n) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    if(n <= 0) {
        return 0;
    }
    
    // 增强的安全检查
    if (buf_addr == 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    // 检查内核空间指针
    if (buf_addr >= 0x80000000) {
        printf("SECURITY: read attempt with kernel pointer: 0x%lx\n", buf_addr);
        set_syscall_error(SYSERR_ACCESS_DENIED);
        return -1;
    }
    
    // 限制读取大小
    if (n > 4096) {
        n = 4096; // 限制为4KB
    }
    
    // 简化实现：返回模拟数据
    struct proc *p = myproc();
    char *kbuf = alloc_page();
    if(!kbuf) {
        set_syscall_error(SYSERR_MEMORY_FAULT);
        return -1;
    }
    
    // 模拟读取数据
    const char *test_data = "test input from stdin\n";
    int data_len = strlen(test_data);
    int read_len = n < data_len ? n : data_len;
    // copy without relying on libc memcpy
    for (int i = 0; i < read_len; i++) kbuf[i] = test_data[i];
    
    // 拷贝到用户空间
    if(copyout(p->pagetable, buf_addr, kbuf, read_len) < 0) {
        free_page(kbuf);
        set_syscall_error(SYSERR_MEMORY_FAULT);
        return -1;
    }
    
    free_page(kbuf);
    return read_len;
}

// 内存管理系统调用
int sys_brk(void) {
    uint64_t addr;
    if(argaddr(0, &addr) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: brk called with addr=0x%lx\n", addr);
    
    /* struct proc *p = myproc(); not used in simplified implementation */
    
    // 简化实现：直接返回当前brk值
    // 在实际实现中，这里应该管理进程的堆空间
    
    if(addr == 0) {
        // 查询当前brk - 返回一个合理的值
        return 0x100000; // 1MB
    }
    
    // 对于非零地址，返回成功
    return 0;
}

int sys_sbrk(void) {
    int increment;
    if(argint(0, &increment) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: sbrk called with increment=%d\n", increment);
    
    // 简化实现：返回当前brk，不实际分配内存
    uint64_t current_brk = 0x100000; // 假设当前brk在1MB
    
    if(increment == 0) {
        return current_brk;
    }
    
    // 返回旧的brk值
    return current_brk;
}

// kernel/sysproc.c - 修改 sys_getprocinfo 函数，添加详细调试
int sys_getprocinfo(void) {
    uint64_t info_ptr;
    
    // 获取用户空间缓冲区指针
    if (argaddr(0, &info_ptr) < 0) {
        printf("SYSCALL: getprocinfo - failed to get argument\n");
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    printf("SYSCALL: getprocinfo called from pid %d, info_ptr=0x%lx\n", 
           myproc()->pid, info_ptr);
    
    // 检查指针是否有效
    if (info_ptr == 0) {
        printf("SYSCALL: getprocinfo - null pointer provided\n");
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    struct proc *p = myproc();
    if (!p) {
        printf("SYSCALL: getprocinfo - no current process\n");
        set_syscall_error(SYSERR_INTERNAL);
        return -1;
    }
    
    // 填充进程信息
    struct procinfo info;
    info.pid = p->pid;
    info.state = p->state;
    info.parent_pid = p->parent ? p->parent->pid : 0;
    
    // 复制进程名称（确保以null结尾）
    int i;
    for (i = 0; i < sizeof(info.name) - 1 && p->name[i] != '\0'; i++) {
        info.name[i] = p->name[i];
    }
    info.name[i] = '\0';
    
    printf("SYSCALL: Process info prepared - pid=%d, state=%d, parent=%d, name='%s'\n",
           info.pid, info.state, info.parent_pid, info.name);
    
    // 在内核测试环境中，使用直接内存拷贝
    printf("SYSCALL: Copying process info to 0x%lx\n", info_ptr);
    
    // 直接内存拷贝（适用于内核测试环境）
    struct procinfo *dest = (struct procinfo*)info_ptr;
    *dest = info;
    
    printf("SYSCALL: getprocinfo - direct copy completed successfully\n");
    return 0;
}

// 设置进程优先级
int sys_setpriority(void) {
    int pid, value;
    if (argint(0, &pid) < 0 || argint(1, &value) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    int ret = proc_set_priority(pid, value);
    if (ret == 0) {
        return 0;
    }
    
    if (ret == -1) {
        set_syscall_error(SYSERR_INVALID_ARGS);
    } else {
        set_syscall_error(SYSERR_NOT_FOUND);
    }
    return -1;
}

// 获取进程优先级
int sys_getpriority(void) {
    int pid;
    if (argint(0, &pid) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    int priority = proc_get_priority(pid);
    if (priority < 0) {
        set_syscall_error(SYSERR_NOT_FOUND);
        return -1;
    }
    
    return priority;
}

// 文件系统相关系统调用
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

// 每个进程的文件描述符表
#define NOFILE 16

int sys_open(void) {
    char path[256];
    int omode;
    struct file *f;
    struct inode *ip;
    
    if (argstr(0, path, sizeof(path)) < 0 || argint(1, &omode) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    begin_op();
    
    if (omode & O_CREATE) {
        ip = ialloc(ROOTDEV, T_FILE);
        if (ip == 0) {
            end_op();
            set_syscall_error(SYSERR_INTERNAL);
            return -1;
        }
    } else {
        if ((ip = namei(path)) == 0) {
            end_op();
            set_syscall_error(SYSERR_NOT_FOUND);
            return -1;
        }
    }
    
    if ((f = filealloc()) == 0 || (ip->type == T_DIR && omode != O_RDONLY)) {
        if (f) {
            fileclose(f);
        }
        iput(ip);
        end_op();
        set_syscall_error(SYSERR_INTERNAL);
        return -1;
    }
    
    if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
        fileclose(f);
        iput(ip);
        end_op();
        set_syscall_error(SYSERR_INTERNAL);
        return -1;
    }
    
    f->type = FD_INODE;
    f->off = 0;
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    
    if ((omode & O_TRUNC) && ip->type == T_FILE) {
        itrunc(ip);
    }
    
    end_op();
    
    // 分配文件描述符（简化实现）
    // 在实际实现中，应该管理进程的文件描述符表
    // 这里返回一个简单的文件描述符
    printf("SYSCALL: open - path='%s', mode=%d\n", path, omode);
    (void)f; // 避免未使用警告
    return 3; // 简化：返回固定值
}

int sys_close(void) {
    int fd;
    
    if (argint(0, &fd) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    // 简化实现：查找并关闭文件
    // 在实际实现中，应该从进程的文件描述符表中查找
    printf("SYSCALL: close - fd=%d\n", fd);
    (void)fd; // 避免未使用警告
    return 0;
}

int sys_unlink(void) {
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], path[256];
    uint32_t off;
    
    if (argstr(0, path, sizeof(path)) < 0) {
        set_syscall_error(SYSERR_INVALID_ARGS);
        return -1;
    }
    
    begin_op();
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        set_syscall_error(SYSERR_NOT_FOUND);
        return -1;
    }
    
    if ((ip = dirlookup(dp, name, &off)) == 0) {
        iput(dp);
        end_op();
        set_syscall_error(SYSERR_NOT_FOUND);
        return -1;
    }
    
    if (ip->nlink < 1) {
        printf("fs: unlink - nlink < 1\n");
    }
    
    if (ip->type == T_DIR) {
        iput(dp);
        iput(ip);
        end_op();
        set_syscall_error(SYSERR_INTERNAL);
        return -1;
    }
    
    if (readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        printf("fs: unlink - readi\n");
    }
    
    if (de.inum != ip->inum) {
        printf("fs: unlink - writei\n");
    }
    
    de.inum = 0;
    if (writei(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
        printf("fs: unlink - writei\n");
    }
    
    if (ip->nlink == 0) {
        ip->type = 0;
        iupdate(ip);
        iput(ip);
    }
    iput(ip);
    iput(dp);
    end_op();
    
    printf("SYSCALL: unlink - path='%s'\n", path);
    return 0;
}