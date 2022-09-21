[ORG 0x7c00]
[BITS 16]

xor ax, ax
mov ds, ax
mov es, ax
mov ah, 0x42
mov si, addr_packet
int 0x13
jnc 0x8000
push 0xb800
pop es
xor di, di
mov si, string
mov cx, 10
mov ah, 0x7
err_print:
lodsb
stosw
loop err_print
hlt_loop:
hlt
jmp hlt_loop

string: db 'DISK ERROR'

addr_packet:
db 0x10, 0x00 ; size, reserved
dw 0x39 ; blocks
dd 0x8000 ; transfer buffer
dq 1 ; start block
