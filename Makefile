PREFIX = riscv64-unknown-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld

# -D_GNU_SOURCE is often needed for mquickjs
# -fno-math-errno -fno-trapping-math are suggested in mquickjs makefile
CFLAGS = -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -Iinclude -O2 -g -Wall \
         -D_GNU_SOURCE -fno-math-errno -fno-trapping-math

LDFLAGS = -T link.ld -nostartfiles -nostdlib

SRCS = js.c uart.c mquickjs.c cutils.c dtoa.c libm.c libc_stubs.c
ASRCS = startup.S
OBJS = $(SRCS:.c=.o) $(ASRCS:.S=.o)

firmware.elf: $(OBJS) link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_EXTRA) -o $@ $(OBJS) -lgcc

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) firmware.elf qemu_debug.log

run: firmware.elf
	qemu-system-riscv64 -machine virt -m 256 -nographic -bios firmware.elf

debug: firmware.elf
	qemu-system-riscv64 -machine virt -m 256 -nographic -bios firmware.elf -d in_asm,cpu_reset -D qemu_debug.log
