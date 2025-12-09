# qemu -kernel loads the kernel at 0x80000000
# and causes the CPU to jump there.

.section .text
.global _entry
_entry:
        # Set up stack for C code
        # For single-core system, we use a fixed stack
        la sp, stack0           # Load address of stack0
        li t0, 4096             # Stack size = 4096 bytes
        add sp, sp, t0          # Set stack pointer to top of stack

        # Clear BSS section (important for uninitialized variables)
        la a0, _bss_start       # Start of BSS
        la a1, _bss_end         # End of BSS
        j clear_bss_loop_test

clear_bss_loop:
        sw zero, 0(a0)          # Store zero to clear BSS
        addi a0, a0, 4          # Move to next word

clear_bss_loop_test:
        blt a0, a1, clear_bss_loop  # Continue until end of BSS

        # Initialize memory management system first
        call pmm_init           # Initialize physical memory manager
        call kvminit            # Create kernel page table
        
        # Enable virtual memory
        call kvminithart        # Enable paging

        # Jump to C main function
        call main

spin:
        j spin                  # Infinite loop if main returns