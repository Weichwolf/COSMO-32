PREFIX = riscv64-unknown-elf-
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

ASFLAGS = -march=rv32imac_zicsr -mabi=ilp32
LDFLAGS = -T test.ld -m elf32lriscv

TESTS = basic mul branch compressed atomic usart timer interrupt dma fsmc display i2s eth icmp dhcp tftp tftp_read tftp_write tftp_rw

all: $(addsuffix .bin,$(TESTS))

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.elf: %.o test.ld
	$(LD) $(LDFLAGS) -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.dump: %.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

clean:
	rm -f *.o *.elf *.bin *.dump

.PHONY: all clean
