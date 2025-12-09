# kernel/trap_entry.S
#这部分代码构成了完整的上下文切换机制，在陷阱发生时：
# 保存当前执行状态到栈上
# 调用 C 语言陷阱处理函数
# 恢复之前的状态并返回
.section .text
.global trap_vector
.align 4
trap_vector:
    # 保存所有需要保存的寄存器到栈上
    addi sp, sp, -264        # 增加空间以保存特殊寄存器
    
    # 保存通用寄存器
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd t0, 24(sp)
    sd t1, 32(sp)
    sd t2, 40(sp)
    sd s0, 48(sp)
    sd s1, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)
    sd s2, 128(sp)
    sd s3, 136(sp)
    sd s4, 144(sp)
    sd s5, 152(sp)
    sd s6, 160(sp)
    sd s7, 168(sp)
    sd s8, 176(sp)
    sd s9, 184(sp)
    sd s10, 192(sp)
    sd s11, 200(sp)
    sd t3, 208(sp)
    sd t4, 216(sp)
    sd t5, 224(sp)
    sd t6, 232(sp)
    
    # 保存 sp
    addi t0, sp, 264
    sd t0, 240(sp)
    
    # 保存特殊寄存器
    csrr t0, mepc
    sd t0, 248(sp)
    csrr t0, mstatus
    sd t0, 256(sp)
    # mtval 在 trap_handler 中读取
    
    # 设置参数（栈指针）给 C 处理函数
    mv a0, sp
    
    # 调用 C 处理函数
    call trap_handler
    
    # 恢复特殊寄存器
    ld t0, 248(sp)
    csrw mepc, t0
    ld t0, 256(sp)
    csrw mstatus, t0
    
    # 恢复通用寄存器
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld t0, 24(sp)
    ld t1, 32(sp)
    ld t2, 40(sp)
    ld s0, 48(sp)
    ld s1, 56(sp)
    ld a0, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)
    ld s2, 128(sp)
    ld s3, 136(sp)
    ld s4, 144(sp)
    ld s5, 152(sp)
    ld s6, 160(sp)
    ld s7, 168(sp)
    ld s8, 176(sp)
    ld s9, 184(sp)
    ld s10, 192(sp)
    ld s11, 200(sp)
    ld t3, 208(sp)
    ld t4, 216(sp)
    ld t5, 224(sp)
    ld t6, 232(sp)
    
    # 恢复 sp
    ld sp, 240(sp)
    
    # 返回
    mret