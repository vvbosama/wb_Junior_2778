CROSS_COMPILE ?= riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
NM      = $(CROSS_COMPILE)nm

CFLAGS  = -Wall -O2 -ffreestanding -fno-builtin -nostdlib -nostartfiles \
          -march=rv64imac -mabi=lp64 -mcmodel=medany -I include
LDFLAGS = -T kernel/linker.ld

# 源文件：加入 console.c / printf.c / test.c
SRCS = \
  kernel/entry.S \
  kernel/main.c \
  kernel/uart.c \
  kernel/console.c \
  kernel/printf.c \
  test.c

# 由 SRCS 推导目标
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

.PHONY: all clean run dump

all: kernel.elf

# 统一的模式规则
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# 链接时让 ld 看到链接脚本变化
kernel.elf: $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) kernel.elf kernel_bare.elf

run: kernel.elf
	@echo "Running bare-metal kernel (no OpenSBI) ..."
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

dump: kernel.elf
	$(OBJDUMP) -h kernel.elf
	$(NM) -n kernel.elf | grep -E "(_start|kernel_main|__edata|__end|stack_top|etext)"
