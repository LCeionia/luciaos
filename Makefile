objects = entry.o kernel.o task.o handler.o interrupt.o v86.o print.o tss.o dosfs/dosfs.o
CFLAGS = -target "i686-elf" -m32 -mgeneral-regs-only -ffreestanding -march=pentium-m -fno-stack-protector -nostdlib -c

%.o: %.nasm
	nasm -f elf32 -o $@ $<

%.o: %.c
	clang $(CFLAGS) -O2 -o $@ $<

all: $(objects)
	nasm boot.nasm -o boot.bin
	gcc -Tlink.ld -m32 -ffreestanding -nostartfiles -nostdlib -o kernel.bin\
		$(objects)
	dd bs=256 count=1 conv=notrunc if=boot.bin of=virtdisk.bin
	dd bs=512 seek=1 conv=notrunc if=kernel.bin of=virtdisk.bin
virtdisk:
	dd bs=1M count=32 if=/dev/zero of=virtdisk.bin
	echo -n -e '\x55\xaa' | dd bs=1 seek=510 conv=notrunc of=virtdisk.bin

clean:
	rm $(objects)
