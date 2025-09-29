# Cross toolchain
CROSS_COMPILE ?= riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
NM      = $(CROSS_COMPILE)nm

# Common flags
CFLAGS  = -Wall -O2 -ffreestanding -fno-builtin -nostdlib -nostartfiles \
          -march=rv64imac -mabi=lp64 -mcmodel=medany \
          -I kernel -I include
LDFLAGS = -T kernel/linker.ld

# Sources
SRCS = \
  kernel/entry.S \
  kernel/main.c  \
  kernel/uart.c  \
  kernel/console.c \
  kernel/printf.c  \
  kernel/kalloc.c  \
  kernel/vm.c      \
  kernel/tests.c

# Objects inferred from SRCS
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

.PHONY: all clean run dump qemu

all: kernel.elf

# Compile rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# Link
kernel.elf: $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) kernel.elf

run qemu: kernel.elf
	@echo "Running bare-metal kernel (no OpenSBI) ..."
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

dump: kernel.elf
	$(OBJDUMP) -h kernel.elf
	$(NM) -n kernel.elf | grep -E "(_start|kernel_main|__edata|__end|stack_top|etext)"
