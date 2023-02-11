objects = entry.o kernel.o task.o handler.o interrupt.o v86.o print.o tss.o dosfs/dosfs.o gdt.o\
		  usermode.o paging.o fault.o tests.o kbd.o helper.o progs.o disk.o hexedit.o
CFLAGS = -target "i686-elf" -m32 -mgeneral-regs-only -ffreestanding\
		 -march=i686 -fno-stack-protector -Wno-int-conversion -nostdlib -c
LFLAGS = -Wl,--gc-sections -Wl,--print-gc-sections -m32 -nostartfiles -nostdlib

ifeq ($(OUTFILE),)
OUTFILE = virtdisk.bin
endif

.PHONY: $(OUTFILE)

all: $(OUTFILE)

$(OUTFILE): boot.bin kernel.bin
	# Copy to boot sector, don't overwrite MBR
	dd bs=400 count=1 conv=notrunc if=boot.bin of=$@
	# Write kernel beyond boot sector, maximum 128K (256 sectors)
	dd bs=512 count=256 seek=1 conv=notrunc if=kernel.bin of=$@

boot.bin: boot.nasm
	nasm -o $@ $<

kernel.bin: out.o link.ld
	clang $(LFLAGS) -Wl,-M -Tlink.ld -ffreestanding -o $@ $<

out.o: $(objects)
	clang $(LFLAGS) -e entry -r -o $@ $^

%.o: %.nasm
	nasm -f elf32 -o $@ $<

%.o: %.c
	clang $(CFLAGS) -ffunction-sections -fdata-sections -Os -o $@ $<

virtdisk:
	dd bs=1M count=32 if=/dev/zero of=virtdisk.bin
	echo -n -e '\x55\xaa' | dd bs=1 seek=510 conv=notrunc of=virtdisk.bin

clean:
	rm -f $(objects) out.o kernel.bin boot.bin
