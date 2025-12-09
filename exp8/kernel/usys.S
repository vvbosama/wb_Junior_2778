# user/usys.S
# 系统调用桩代码

.section .text

# 系统调用宏定义
.macro SYSCALL name number
.global \name
\name:
    li a7, \number
    ecall
    ret
.endm

# 定义所有系统调用
SYSCALL fork, 1
SYSCALL exit, 2
SYSCALL wait, 3
SYSCALL kill, 4
SYSCALL getpid, 5
SYSCALL write, 6
SYSCALL read, 7
SYSCALL open, 8
SYSCALL close, 9
SYSCALL brk, 10
SYSCALL sbrk, 11
SYSCALL exec, 12
SYSCALL chdir, 13
SYSCALL dup, 14
SYSCALL getcwd, 15
SYSCALL sleep, 16
SYSCALL uptime, 17
SYSCALL getppid, 18
SYSCALL getprocinfo, 19
SYSCALL setpriority, 21
SYSCALL getpriority, 22