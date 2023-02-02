oldscancodesToAscii: db 0, 0 ; 0x00 - 0x01
db "1234567890" ; 0x02 - 0x0B
db "-=" ; 0x0C - 0x0D
db 0, 0 ; 0x0E - 0x0F
db "qwertyuiop[]" ; 0x10 - 0x1B
db 0, 0 ; 0x1C - 0x1D
db "asdfghjkl;'`" ; 0x1E - 0x29
db 0 ; 0x2A
db "\zxcvbnm,./" ; 0x2B - 0x35
db 0 ; 0x36
db '*' ; 0x37
db 0 ; 0x38
db ' ' ; 0x39
db 'C'
scancodesToAsciiEnd:
cursorCurrent: dd 0xb8000 + (80*6*2)
global oldkeyboardHandler
oldkeyboardHandler:
push eax
push ebx
push ds
mov ax, 0x10
mov ds, ax
xor eax, eax
in al, 0x60
cmp eax, 0x3A
jg .done
mov al, [oldscancodesToAscii+eax]
test al, al
jz .done
mov ebx, [cursorCurrent]
mov byte [ebx], al
add dword [cursorCurrent], 2
mov byte [KBDWAIT], 1
.done:
mov al, 0x20
out 0x20, al
pop ds
pop ebx
pop eax
iret

KBDWAIT: db 0
oldkbd_wait:
mov byte [KBDWAIT], 0
.loop:
hlt
movzx eax, byte [KBDWAIT]
test eax, eax
jz .loop
ret

TIMERVAL: dd 0
global timerHandler
timerHandler:
push eax
push ds
mov ax, 0x10
mov ds, ax
;inc byte [(0xb8000 + (80*8*2))]
inc dword [TIMERVAL]
mov al, 0x20
out 0x20, al
pop ds
pop eax
iret

global picInit
picInit:
mov al, 0x11  ; initialization sequence
out 0x20, al  ; send to 8259A-1
jmp $+2
jmp $+2
out 0xA0, al  ; and to 8259A-2
jmp $+2
jmp $+2
mov al, 0x20  ; start of hardware ints (0x20)
out 0x21, al
jmp $+2
jmp $+2
mov al, 0x28  ; start of hardware ints 2 (0x28)
out 0xA1, al
jmp $+2
jmp $+2
mov al, 0x04  ; 8259-1 is master
out 0x21, al
jmp $+2
jmp $+2
mov al, 0x02  ; 8259-2 is slave
out 0xA1, al
jmp $+2
jmp $+2
mov al, 0x01  ; 8086 mode for both
out 0x21, al
jmp $+2
jmp $+2
out 0xA1, al
jmp $+2
jmp $+2
mov al, 0xFF  ; all interrupts off for now
out 0x21, al
jmp $+2
jmp $+2
out 0xA1, al
ret
