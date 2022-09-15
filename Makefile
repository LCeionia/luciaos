#TODO make the makefile use targets instead of this mess
all:
	nasm boot.nasm -o boot.bin
	nasm -f elf32 -o entry.o entry.nasm
	nasm -f elf32 -o handler.o handler.nasm
	nasm -f elf32 -o v86.o v86.nasm
	clang -target "i686-elf" -ffreestanding -m32 -O2 -march=pentium-m -fno-stack-protector -nostdlib -c kernel.c
	clang -target "i686-elf" -ffreestanding -m32 -O2 -mgeneral-regs-only -march=pentium-m -fno-stack-protector -nostdlib -c print.c
	# not sure why but if interrupt.c has any optimization everything just breaks immediately
	clang -target "i686-elf" -ffreestanding -m32 -mgeneral-regs-only -march=pentium-m -fno-stack-protector -nostdlib -c interrupt.c
	#clang -target "i686-elf" -ffreestanding -m32 -mgeneral-regs-only -march=i386 -fno-stack-protector -nostdlib -c -o kernel.o kernel.c
	gcc -Tlink.ld -m32 -ffreestanding -nostartfiles -nostdlib -o kernel.bin entry.o v86.o handler.o kernel.o print.o interrupt.o
	dd bs=256 count=1 conv=notrunc if=boot.bin of=virtdisk.bin
	dd bs=512 seek=1 conv=notrunc if=kernel.bin of=virtdisk.bin
virtdisk:
	dd bs=1M count=32 if=/dev/zero of=virtdisk.bin
	echo -n -e '\x55\xaa' | dd bs=1 seek=510 conv=notrunc of=virtdisk.bin
