# Fix for MSYS2 UCRT64 environment
export MSYSTEM := UCRT64
export TMPDIR := /tmp
export TMP := /tmp
export TEMP := /tmp

PREFIX   := riscv64-unknown-elf-
AS       := $(PREFIX)as
CC       := $(PREFIX)gcc
LD       := $(PREFIX)ld
OBJCOPY  := $(PREFIX)objcopy
OBJDUMP  := $(PREFIX)objdump

ARCH     := -march=rv32imac -mabi=ilp32

ASFLAGS  := $(ARCH) -Isrc
CFLAGS   := $(ARCH) -O2 -ffreestanding -nostdlib -Wall -Wextra -Isrc
LDFLAGS  := -m elf32lriscv -T link.ld -nostdlib

SRC_S    := $(wildcard src/*.S)
SRC_C    := $(wildcard src/*.c)
OBJ      := $(SRC_S:.S=.o) $(SRC_C:.c=.o)

.PHONY: all clean dump

all: firmware.bin

firmware.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

firmware.bin: firmware.elf
	$(OBJCOPY) -O binary $< $@

# .S files go through gcc (preprocessor + assembler)
%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

dump: firmware.elf
	$(OBJDUMP) -d -M no-aliases $<

clean:
	rm -f $(OBJ) firmware.elf firmware.bin
