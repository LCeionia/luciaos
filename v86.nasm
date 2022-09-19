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
global v86Test
v86Test:
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
int 3
int 3
int 0x30 ; exit
jmp $
global v86GfxMode
v86GfxMode:
mov ax, 0x13
int 0x10
int 0x30
jmp $
[BITS 32]
; extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
global enter_v86
enter_v86:
pop eax
mov ebp, esp               ; save stack pointer
call save_current_task
push dword  [ebp+0]        ; ss
push dword  [ebp+4]        ; esp
pushfd                     ; eflags
or dword [esp], (1 << 17)  ; set VM flags
;or dword [esp], (3 << 12) ; IOPL 3
push dword [ebp+8]        ; cs
push dword  [ebp+12]       ; eip
iret

; return address in eax, return stack in ebp
extern save_current_task
