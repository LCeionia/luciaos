[BITS 16]
real_hexprint:
xor cx, cx
mov bl, al
shr al, 4
jmp .donibble
.nibble2:
mov al, bl
inc cx
.donibble:
and al, 0x0F
cmp al, 0x0A
jl .noadjust
add al, 'A' - '0' - 10
.noadjust:
add al, '0'
mov ah, 0x1f
stosw
test cx, cx
jz .nibble2
ret
real_printword:
mov dx, ax
mov al, ah
call real_hexprint
mov ax, dx
call real_hexprint
ret
extern v86Code
v86Code:
mov ax, 0xb814
mov es, ax
mov di, 20
mov ax, sp
call real_printword
add di, 2
mov ax, ds
call real_printword
add di, 2
mov ax, cs
call real_printword
.loop:
inc byte [0]
;mov ax, 0x1111
;mov ds, ax
;mov ax, 0x2222
;mov es, ax
;mov ax, 0x3333
;mov fs, ax
;mov ax, 0x4444
;mov gs, ax
;mov ax, 0x5555
;mov ss, ax
;mov ax, 0x6666
;mov sp, ax
int 3
int 3
;jmp .loop
mov ax, 0x13
int 0x10
int 0x30 ; exit
jmp $
extern real_test
real_test:
nop
nop
nop
jmp $
[BITS 32]
; extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
global enter_v86
enter_v86:
mov ebp, esp               ; save stack pointer
push dword  [ebp+4]        ; ss
push dword  [ebp+8]        ; esp
pushfd                     ; eflags
or dword [esp], (1 << 17)  ; set VM flags
;or dword [esp], (3 << 12) ; IOPL 3
push dword [ebp+12]        ; cs
push dword  [ebp+16]       ; eip
iret

