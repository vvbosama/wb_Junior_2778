#include "printf.h"
#include "proc.h"
#include "mm.h"
#include "console.h"
#include "trap.h"
#include "string.h"
#include "sysproc.h"
#include "syscall.h"

// 系统调用表定义（使用 include/syscall.h 中的声明/类型）
struct syscall_desc syscall_table[SYSCALL_MAX] = {
    [SYS_fork]    = {sys_fork,    "fork",    0, 0, 0},
    [SYS_exit]    = {sys_exit,    "exit",    1, 0x1, 0}, // ARG_INT
    [SYS_wait]    = {sys_wait,    "wait",    1, 0x2, 0}, // ARG_PTR  
    [SYS_kill]    = {sys_kill,    "kill",    1, 0x1, 0},
    [SYS_getpid]  = {sys_getpid,  "getpid",  0, 0, 0},
    [SYS_write]   = {sys_write,   "write",   3, 0x1 | (0x2 << 4) | (0x1 << 8), 0},
    [SYS_read]    = {sys_read,    "read",    3, 0x1 | (0x2 << 4) | (0x1 << 8), 0},
    [SYS_open]    = {0,           "open",    2, 0x2 | (0x1 << 4), 0},
    [SYS_close]   = {0,           "close",   1, 0x1, 0},
    [SYS_brk]     = {sys_brk,     "brk",     1, 0x2, 0},
    [SYS_sbrk]    = {sys_sbrk,    "sbrk",    1, 0x1, 0},
    [SYS_exec]    = {0,           "exec",    2, 0x2 | (0x2 << 4), 0},
    [SYS_getppid] = {sys_getppid, "getppid", 0, 0, 0},
    [SYS_getprocinfo] = {sys_getprocinfo, "getprocinfo", 1, 0x2, 0},  // 新增
};

// 使用 include/syscall.h 中定义的 syscall_result_t

static int last_error = SYSERR_SUCCESS;

// myproc is provided by sysproc.c (kernel/sysproc.c)

// 参数提取辅助函数
uint64_t argraw(int n) {
    struct proc *p = myproc();
    if (!p || !p->trap_context) {
        return 0;
    }
    
    // 从陷阱上下文中获取参数
    switch (n) {
    case 0: return p->trap_context->a0;
    case 1: return p->trap_context->a1;
    case 2: return p->trap_context->a2;
    case 3: return p->trap_context->a3;
    case 4: return p->trap_context->a4;
    case 5: return p->trap_context->a5;
    case 6: return p->trap_context->a6;
    case 7: return p->trap_context->a7;
    }
    return 0;
}

int argint(int n, int *ip) {
    if (n < 0 || n > 5) {
        return -1;
    }
    *ip = (int)argraw(n);
    return 0;
}

int argaddr(int n, uint64_t *ip) {
    if (n < 0 || n > 5) {
        return -1;
    }
    *ip = argraw(n);
    return 0;
}

int argstr(int n, char *buf, int max) {
    uint64_t addr;
    if(argaddr(n, &addr) < 0) {
        return -1;
    }
    return fetchstr(addr, buf, max);
}

// 用户内存访问函数
int fetchstr(uint64_t addr, char *buf, int max) {
    struct proc *p = myproc();
    if (!p) {
        return -1;
    }
    
    // 简化实现：假设所有用户地址都是有效的
    // 在实际系统中需要检查地址范围
    if (addr == 0 || max <= 0) {
        return -1;
    }
    
    int i;
    for(i = 0; i < max - 1; i++) {
        char c;
        if(copyin(p->pagetable, &c, addr + i, 1) < 0) {
            break;
        }
        buf[i] = c;
        if(c == '\0') {
            return i;
        }
    }
    
    buf[max-1] = '\0';
    return -1;
}

// 在copyin/copyout函数中添加基本边界检查
int copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    
    // 增强的安全检查
    if (srcva < 0x1000) { // 避免访问NULL指针区域
        return -1;
    }
    
    // 检查内核空间访问
    if (srcva >= 0x80000000) {
        printf("SECURITY: Attempt to copy from kernel space: 0x%lx\n", srcva);
        return -1;
    }
    
    // 在实际系统中应该检查页表权限，这里简化实现
    char *src = (char*)srcva;
    
    // 添加长度限制
    if (len > 4096) { // 限制单次拷贝大小
        printf("SECURITY: Oversized copyin attempt: %lu bytes\n", len);
        return -1;
    }
    
    // simple byte copy
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];
    return 0;
}

int copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    
    // 增强的安全检查
    if (dstva < 0x1000) { // 避免访问NULL指针区域
        return -1;
    }
    
    // 检查内核空间访问
    if (dstva >= 0x80000000) {
        printf("SECURITY: Attempt to copy to kernel space: 0x%lx\n", dstva);
        return -1;
    }
    
    // 添加长度限制
    if (len > 4096) { // 限制单次拷贝大小
        printf("SECURITY: Oversized copyout attempt: %lu bytes\n", len);
        return -1;
    }
    
    char *dst = (char*)dstva;
    
    // simple byte copy
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];
    return 0;
}

// 虚拟地址到物理地址的转换（简化版）
uint64_t walkaddr(pagetable_t pagetable, uint64_t va) {
    // 简化实现：直接返回虚拟地址
    // 在实际系统中需要遍历页表
    return va;
}

// 错误处理
void set_syscall_error(int err) {
    last_error = err;
}

int get_last_syscall_error(void) {
    return last_error;
}

const char *syscall_error_str(int err) {
    switch(err) {
        case SYSERR_SUCCESS: return "Success";
        case SYSERR_INVALID_ARGS: return "Invalid arguments";
        case SYSERR_ACCESS_DENIED: return "Access denied";
        case SYSERR_MEMORY_FAULT: return "Memory fault";
        case SYSERR_RESOURCE_BUSY: return "Resource busy";
        case SYSERR_NOT_FOUND: return "Not found";
        case SYSERR_NOT_SUPPORTED: return "Not supported";
        case SYSERR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

// 权限检查
int check_syscall_permission(struct proc *p, int syscall_num) {
    if (!p) {
        return 0;
    }
    
    // 简化实现：所有系统调用都允许
    // 在实际系统中需要根据进程权限和系统调用类型进行检查
    
    // 检查系统调用号范围
    if (syscall_num < 0 || syscall_num >= SYSCALL_MAX) {
        return 0;
    }
    
    // 检查系统调用是否实现
    if (!syscall_table[syscall_num].func) {
        return 0;
    }
    
    return 1;
}

// 系统调用分发器
void syscall_dispatch(struct trap_context *ctx) {
    struct proc *p = myproc();
    if (!p) {
        // 如果没有当前进程，设置错误并返回
        printf("SYSCALL: no current process\n");
        ctx->a0 = -1;
        return;
    }
    
    // 保存陷阱上下文到进程结构中
    p->trap_context = ctx;
    
    uint64_t syscall_num = ctx->a7;
     printf("DEBUG: syscall_dispatch called, num=%lu, pid=%d\n", 
           syscall_num, p->pid);
           
    syscall_result_t result = {0, SYSERR_SUCCESS};
    
    // 重置错误状态
    set_syscall_error(SYSERR_SUCCESS);
    
    // 系统调用号验证
    if (syscall_num >= SYSCALL_MAX || !syscall_table[syscall_num].func) {
        printf("SYSCALL: unknown syscall %d from pid %d\n", 
               (int)syscall_num, p->pid);
        result.error = SYSERR_NOT_SUPPORTED;
        ctx->a0 = -1;
        return;
    }
    
    // 权限检查
    if (!check_syscall_permission(p, syscall_num)) {
        printf("SYSCALL: permission denied for %s from pid %d\n",
               syscall_table[syscall_num].name, p->pid);
        result.error = SYSERR_ACCESS_DENIED;
        ctx->a0 = -1;
        return;
    }
    
    printf("SYSCALL: %s called from pid %d\n", 
           syscall_table[syscall_num].name, p->pid);
    
    // 执行系统调用
    result.return_value = syscall_table[syscall_num].func();
    result.error = get_last_syscall_error();
    
    // 设置返回值
    if (result.error != SYSERR_SUCCESS) {
        ctx->a0 = -1;
        printf("SYSCALL: %s failed with error: %s\n",
               syscall_table[syscall_num].name, 
               syscall_error_str(result.error));
    } else {
        ctx->a0 = result.return_value;
     printf("SYSCALL: %s returning %ld\n",
         syscall_table[syscall_num].name, result.return_value);
    }
    
    // 清除陷阱上下文引用
    p->trap_context = NULL;
}

// 系统调用初始化
void syscall_init(void) {
    // 初始化系统调用表
    int implemented_count = 0;
    for (int i = 0; i < SYSCALL_MAX; i++) {
        if (syscall_table[i].func) {
            implemented_count++;
        }
    }
    
    printf("System calls: initialized with %d/%d syscalls implemented\n", 
           implemented_count, SYSCALL_MAX);
    
    // 打印实现的系统调用列表
    printf("Implemented syscalls: ");
    for (int i = 0; i < SYSCALL_MAX; i++) {
        if (syscall_table[i].func) {
            printf("%s(%d) ", syscall_table[i].name, i);
        }
    }
    printf("\n");
}

// 未实现的系统调用占位函数
static int sys_not_implemented(void) {
    printf("SYSCALL: system call not implemented\n");
    set_syscall_error(SYSERR_NOT_SUPPORTED);
    return -1;
}

// 初始化未实现的系统调用
void syscall_init_unimplemented(void) {
    for (int i = 0; i < SYSCALL_MAX; i++) {
        if (!syscall_table[i].func && syscall_table[i].name) {
            syscall_table[i].func = sys_not_implemented;
        }
    }
}

// 系统调用测试函数
void test_syscall_framework(void) {
    printf("=== Testing System Call Framework ===\n");
    
    // 测试参数提取
    printf("Testing argument extraction...\n");
    
    // 测试错误处理
    printf("Testing error handling...\n");
    set_syscall_error(SYSERR_INVALID_ARGS);
    if (get_last_syscall_error() == SYSERR_INVALID_ARGS) {
        printf("✓ Error handling test PASSED\n");
    } else {
        printf("✗ Error handling test FAILED\n");
    }
    
    // 测试权限检查
    printf("Testing permission checks...\n");
    if (check_syscall_permission(myproc(), SYS_getpid)) {
        printf("✓ Permission check test PASSED\n");
    } else {
        printf("✗ Permission check test FAILED\n");
    }
    
    printf("System call framework test completed\n");
}