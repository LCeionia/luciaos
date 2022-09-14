all:
	nasm boot.nasm -o boot.bin
	nasm -f elf32 -o entry.o entry.nasm
	clang -target "i686-elf" -ffreestanding -m32 -march=pentium-m -fno-stack-protector -nostdlib -c -o kernel.o kernel.c
	#clang -target "i686-elf" -O2 -ffreestanding -m32 -march=i386 -fno-stack-protector -nostdlib -c -o kernel.o kernel.c
	gcc -Tlink.ld -m32 -ffreestanding -nostartfiles -nostdlib -o kernel.bin entry.o kernel.o
	dd bs=256 count=1 conv=notrunc if=boot.bin of=virtdisk.bin
	dd bs=512 seek=1 conv=notrunc if=kernel.bin of=virtdisk.bin
virtdisk:
	dd bs=1M count=32 if=/dev/zero of=virtdisk.bin
	echo -n -e '\x55\xaa' | dd bs=1 seek=510 conv=notrunc of=virtdisk.bin
