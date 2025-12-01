// kernel/syscall.h
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "types.h"
#include "proc.h"

// 系统调用号定义
#define SYS_fork     1
#define SYS_exit     2  
#define SYS_wait     3
#define SYS_kill     4
#define SYS_getpid   5
#define SYS_write    6
#define SYS_read     7
#define SYS_open     8
#define SYS_close    9
#define SYS_brk      10
#define SYS_sbrk     11
#define SYS_exec     12
#define SYS_chdir    13
#define SYS_dup      14
#define SYS_getcwd   15
#define SYS_sleep    16
#define SYS_uptime   17
#define SYS_getppid  18
#define SYS_getprocinfo 19  // 新增：获取进程信息
#define SYS_unlink   20     // 删除文件

#define SYSCALL_MAX  64

// 进程信息结构体
struct procinfo {
    int pid;           // 进程ID
    int state;         // 进程状态
    int parent_pid;    // 父进程ID
    char name[16];     // 进程名称
};

// 错误码定义
#define SYSERR_SUCCESS 0
#define SYSERR_INVALID_ARGS -1
#define SYSERR_ACCESS_DENIED -2
#define SYSERR_MEMORY_FAULT -3
#define SYSERR_RESOURCE_BUSY -4
#define SYSERR_NOT_FOUND -5
#define SYSERR_NOT_SUPPORTED -6
#define SYSERR_INTERNAL -7

// 系统调用描述符
struct syscall_desc {
    int (*func)(void);
    char *name;
    int arg_count;
    uint32_t arg_types;
    int permission;
};

// 系统调用结果
typedef struct {
    long return_value;
    int error;
} syscall_result_t;

// 系统调用表声明
extern struct syscall_desc syscall_table[SYSCALL_MAX];

// 系统调用接口
void syscall_init(void);
void syscall_dispatch(struct trap_context *ctx);
int check_syscall_permission(struct proc *p, int syscall_num);

// 在系统调用函数声明部分添加：
int sys_getprocinfo(void);

// 参数提取函数
int argint(int n, int *ip);
int argaddr(int n, uint64_t *ip);
int argstr(int n, char *buf, int max);
uint64_t argraw(int n);

// 用户内存访问
int fetchstr(uint64_t addr, char *buf, int max);
int copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len);
int copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len);
uint64_t walkaddr(pagetable_t pagetable, uint64_t va);

// 错误处理
void set_syscall_error(int err);
int get_last_syscall_error(void);
const char *syscall_error_str(int err);

// 系统调用函数声明
int sys_fork(void);
int sys_exit(void);
int sys_wait(void);
int sys_kill(void);
int sys_getpid(void);
int sys_getppid(void);
int sys_write(void);
int sys_read(void);
int sys_open(void);
int sys_close(void);
int sys_unlink(void);
int sys_brk(void);
int sys_sbrk(void);

// 辅助函数
struct proc* myproc(void);

#endif // _SYSCALL_H_