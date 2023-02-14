objects = entry.o kernel.o task.o handler.o interrupt.o v86.o print.o tss.o dosfs/dosfs.o gdt.o\
		  paging.o fault.o tests.o kbd.o helper.o progs.o disk.o hexedit.o textedit.o
CFLAGS = -target "i686-elf" -m32 -mgeneral-regs-only -ffreestanding\
		 -march=i686 -fno-stack-protector -Wno-int-conversion -nostdlib -c
LFLAGS = -Wl,--gc-sections -Wl,--print-gc-sections -m32 -nostartfiles -nostdlib

ifeq ($(OUTFILE),)
OUTFILE = virtdisk.bin
endif
ifeq ($(PARTSTART),)
PARTSTART = 2048
endif

PARTVBR=$(shell echo "$(PARTSTART) * (2^9)" | bc)
PARTVBRBOOT=$(shell echo "$(PARTVBR) + 90" | bc)
KERNSEC=$(shell echo "$(PARTSTART) + 4" | bc)

.PHONY: $(OUTFILE)

all: $(OUTFILE)

$(OUTFILE): boot.bin boot_partition.bin kernel.bin
	# Copy system bootloader to boot sector, don't overwrite MBR
	dd bs=400 count=1 conv=notrunc if=boot.bin of=$@
	# Ensure partition VBR contains EB 5A
	echo -n -e '\xeb\x5a' | dd bs=1 seek=$(PARTVBR) count=2 conv=notrunc of=$@
	# Copy kernel bootloader to partition VBR
	dd bs=1 count=420 seek=$(PARTVBRBOOT) conv=notrunc if=boot_partition.bin of=$@
	# TODO Check that disk has enough reserved sectors,
	# currently this will overwrite the disk if too few
	# Write kernel beyond boot sector, maximum 64K (128 sectors)
	dd bs=512 count=128 seek=$(KERNSEC) conv=notrunc if=kernel.bin of=$@

kernel.bin: out.o link.ld usermode.o
	clang $(LFLAGS) -Wl,-M -Tlink.ld -ffreestanding -o $@ out.o usermode.o

usermode.o: usermode.bin
	objcopy -I binary -O elf32-i386 -B i386 $< $@

%.bin: %.nasm
	nasm -o $@ $<

out.o: $(objects)
	clang $(LFLAGS) -e entry -r -o $@ $^

%.o: %.nasm
	nasm -f elf32 -o $@ $<

%.o: %.c
	clang $(CFLAGS) -ffunction-sections -fdata-sections -Os -o $@ $<

virtdisk:
	cp virtdisk.bin.ex virtdisk.bin

clean:
	rm -f $(objects) out.o kernel.bin boot.bin usermode.bin
