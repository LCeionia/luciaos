[BITS 16]

global entry
entry:
cli             ; no interrupts
xor ax,ax
mov ds, ax
lgdt [gdt_desc]  ; load gdt register
mov eax, cr0    ; set pmode bit
or al, 1
mov cr0, eax
jmp 08h:Pmodecode

[BITS 32]
Pmodecode:
mov ax, 0x10 ; set up segments
mov ds, ax
mov es, ax
mov ss, ax
mov ebp, 0x400000
mov esp, ebp
mov eax, 0x1f001f00
mov ecx, (80*25)/2
mov edi, 0xb8000
rep stosd
call start
hlt_loop:
hlt
jmp hlt_loop

setup_paging:
mov ecx, 1024*3 ; 3K?
xor eax, eax
mov edi, 0x1000
rep stosd ; zero first 3K for some reason
mov edi, 0x4000
mov eax, 0x800007 ; 8MB + 7
std ; fill backwards
.fill:
stosd
sub eax, 0x1000
jge .fill
cld ; fix direction
mov dword [0x0000], 0x2000 + 7
mov dword [0x0004], 0x3000 + 7
mov eax, 0x1000
mov cr3, eax ; page dir start 0x1000
mov eax, cr0
or eax, 0x80000000
mov cr0, eax ; set paging bit
ret ; flushes pre-fetch queue

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

user_test:
mov dword [0xb8000], 0x0f000f00 | 'U' | 's' << 16
mov dword [0xb8004], 0x0f000f00 | 'e' | 'r' << 16
mov dword [0xb8008], 0x0f000f00 | 'm' | 'o' << 16
mov dword [0xb800C], 0x0f000f00 | 'd' | 'e' << 16
mov word [0xb8010], 0x0f00 | '!'
mov edi, 0xA0000
xor eax, eax
.loop:
mov cx, 320
rep stosb
inc al
cmp eax, 200
jl .loop
xor ebx, ebx
div bl

global jmp_usermode_test
jmp_usermode_test:
mov ax, 0x20 | 3
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov eax, esp
push 0x20 | 3
push eax
pushfd
push 0x18 | 3
push user_test
iret

extern unhandled_handler
unhandled_handler:
mov ax, 0x10
mov ds, ax
mov dword [0xb8000], 0x0f000f00 | 'E' | 'R' << 16
mov dword [0xb8004], 0x0f000f00 | 'R' | 'O' << 16
mov dword [0xb8008], 0x0f000f00 | 'R' | '!' << 16
.hlt:
hlt
jmp .hlt

extern gpf_handler_v86
global gpfHandler
gpfHandler:
iret
mov ax, 0x10
mov ds, ax
;jmp gpf_handler_v86
mov word [0xb8000], 0x0f00 | 'G'
mov word [0xb8002], 0x0f00 | 'P'
mov word [0xb8004], 0x0f00 | 'F'
.hlt:
hlt
jmp .hlt

scancodesToAscii: db 0, 0 ; 0x00 - 0x01
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
global keyboardHandler
keyboardHandler:
push eax
push ebx
push ds
mov ax, 0x10
mov ds, ax
xor eax, eax
in al, 0x60
cmp eax, 0x3A
jg .done
mov al, [scancodesToAscii+eax]
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
global kbd_wait
kbd_wait:
mov byte [KBDWAIT], 0
.loop:
hlt
xor eax, eax
mov al, [KBDWAIT]
test eax, eax
jz .loop
ret

global timerHandler
timerHandler:
push eax
push ds
mov ax, 0x10
mov ds, ax
inc byte [(0xb8000 + (80*8*2))]
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

global flushTSS
flushTSS:
mov ax, 0x28
ltr ax
ret

extern tss_data

extern start

gdt_desc:
    dw gdt_end - gdt
    dd gdt
gdt:
gdt_null: dq 0
gdt_code: dw 0xFFFF, 0     ; bits 0-15 limit (4GB), bits 0-15 base address
          db 0             ; bits 16-23 base address
          db 10011010b     ; access byte
          db 11001111b     ; bits 16-19 limit (4GB), 4 bits flags
          db 0 ; bits 24-31 base address
gdt_data: dw 0xFFFF, 0     ; bits 0-15 limit (4GB), bits 0-15 base address
          db 0             ; bits 16-23 base address
          db 10010010b     ; access byte
          db 11001111b     ; bits 16-19 limit (4GB), 4 bits flags
          db 0 ; bits 24-31 base address
gdt_r3code: dw 0xFFFF, 0     ; bits 0-15 limit (4GB), bits 0-15 base address
          db 0             ; bits 16-23 base address
          db 0xFA     ; access byte
          db 11001111b     ; bits 16-19 limit (4GB), 4 bits flags
          db 0 ; bits 24-31 base address
gdt_r3data: dw 0xFFFF, 0     ; bits 0-15 limit (4GB), bits 0-15 base address
          db 0             ; bits 16-23 base address
          db 0xF2     ; access byte
          db 11001111b     ; bits 16-19 limit (4GB), 4 bits flags
          db 0 ; bits 24-31 base address
gdt_tss:  dw 0x2080, 0x0000;26*4, tss_data     ; bits 0-15 limit (4GB), bits 0-15 base address
          db 0x2             ; bits 16-23 base address
          db 0x89     ; access byte
          db 00000000b     ; bits 16-19 limit (4GB), 4 bits flags
          db 0 ; bits 24-31 base address
gdt_end:
