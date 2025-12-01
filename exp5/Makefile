CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -Iinclude

# 修正源文件列表 - 使用正确的扩展名
SRCS = kernel/entry.S kernel/main.c kernel/uart.c kernel/console.c kernel/printf.c kernel/color_printf.c \
       kernel/mm/pmm.c kernel/mm/vmm.c kernel/mm/buddy.c \
       kernel/trap.c kernel/clock.c kernel/trap_entry.S kernel/exception.c \
	   kernel/proc.c kernel/switch.S kernel/priority.c kernel/priority_test.c

OBJS = $(SRCS:.S=.o)
OBJS := $(OBJS:.c=.o)
DEPS = $(OBJS:.o=.d)

kernel.elf: $(OBJS) kernel/kernel.ld
	$(CC) $(CFLAGS) -T kernel/kernel.ld -o $@ $(OBJS) -lgcc

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

qemu: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

clean:
	rm -f *.elf $(OBJS) $(DEPS) kernel/*.d kernel/mm/*.d

-include $(DEPS)

.PHONY: qemu clean