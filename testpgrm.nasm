[BITS 32]
[ORG 0x800000]
xchg bx,bx
mov edi, 0xB8000
mov ecx, 80*25
mov eax, 0x2f00
rep stosw
mov edi, 0xB8000
mov esi, str0
mov ecx, str0_end - str0
mov eax, 0x2f00
.loop:
lodsb
stosw
loop .loop
mov edi, 0xB8000 + 160 - 2
.key_loop:
mov eax, 1 ; command = 01h, get scancode
int 0x21 ; OS call
cmp al, 0x3B ; F1
je .exit
cmp al, 0x3C ; F2
je .crash
cmp ah, 0x00
je .key_loop
mov byte [edi], ah ; ASCII value
add edi, 2
jmp .key_loop
.exit:
mov eax, 0x1337 ; Exit code/Return value
int 0x30 ; Exit
.crash:
jmp 0x00000000

str0: db "Running usermode program from disk! Press F1 to exit. Press F2 to crash."
str0_end:
