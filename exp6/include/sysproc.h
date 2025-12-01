// kernel/sysproc.h
#ifndef _SYSPROC_H_
#define _SYSPROC_H_

#include "types.h"

// 系统调用错误码定义
#define SYSERR_SUCCESS 0
#define SYSERR_INVALID_ARGS -1
#define SYSERR_ACCESS_DENIED -2
#define SYSERR_MEMORY_FAULT -3
#define SYSERR_RESOURCE_BUSY -4
#define SYSERR_NOT_FOUND -5
#define SYSERR_NOT_SUPPORTED -6
#define SYSERR_INTERNAL -7

// 系统调用函数声明
int sys_fork(void);
int sys_exit(void);
int sys_wait(void);
int sys_kill(void);
int sys_getpid(void);
int sys_getppid(void);
int sys_write(void);
int sys_read(void);
int sys_brk(void);
int sys_sbrk(void);

// 辅助函数声明
struct proc* myproc(void);

#endif // _SYSPROC_H_