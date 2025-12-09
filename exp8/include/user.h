// user/user.h
#ifndef _USER_H_
#define _USER_H_

#include "types.h"

// 系统调用声明
int fork(void);
int exit(int status) __attribute__((noreturn));
int wait(int* status);
int kill(int pid);
int getpid(void);
int getppid(void);
char* sbrk(int increment);
int brk(void* addr);
int exec(char* path, char** argv);
int open(const char* path, int flags);
int close(int fd);
int write(int fd, const void* buf, int count);
int read(int fd, void* buf, int count);
int sleep(int ticks);
int setpriority(int pid, int value);//设置进程优先级
int getpriority(int pid);//获取进程优先级

// 标准库函数
int strlen(const char* s);
char* strcpy(char* dest, const char* src);
void* memset(void* dest, int c, int n);
void* memmove(void* dest, const void* src, int n);
int strcmp(const char* s1, const char* s2);

#endif // _USER_H_