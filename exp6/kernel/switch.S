# kernel/switch.S - 添加上下文切换调试
.section .text
.global context_switch
.align 4

context_switch:
    # 保存调试信息
    addi sp, sp, -32
    sd ra, 0(sp)
    sd a0, 8(sp)
    sd a1, 16(sp)
    
    # 打印调试信息
    # call debug_context_switch  # 如果有调试函数
    
    ld a0, 8(sp)
    ld a1, 16(sp)
    ld ra, 0(sp)
    addi sp, sp, 32

    # 保存当前上下文到old
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    # 恢复新上下文从new
    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    # 重要：在跳转前打印信息
    addi sp, sp, -16
    sd ra, 0(sp)
    
    # 这里可以添加跳转前的调试输出
    
    ld ra, 0(sp)
    addi sp, sp, 16

    # 跳转到新进程的返回地址
    ret