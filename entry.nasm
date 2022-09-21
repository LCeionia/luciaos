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
mov fs, ax
mov gs, ax
mov ebp, 0x400000
mov esp, ebp
mov eax, 0x1f001f00
mov ecx, (80*25)/2
mov edi, 0xb8000
rep stosd ; clear screen
call start
hlt_loop:
hlt
jmp hlt_loop

extern start

; currently unused first 8MB identity paging
; taken from Linux 0.01
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

user_test:
mov dword [0xb8000], 0x0f000f00 | 'U' | 's' << 16
mov dword [0xb8004], 0x0f000f00 | 'e' | 'r' << 16
mov dword [0xb8008], 0x0f000f00 | 'm' | 'o' << 16
mov dword [0xb800C], 0x0f000f00 | 'd' | 'e' << 16
mov word [0xb8010], 0x0f00 | '!'
mov edi, 0xA0000
xor eax, eax
.loop:
mov ecx, 320
rep stosb
inc al
cmp eax, 200
jl .loop
mov eax, 0xA0000
int 0x30 ; Exit
xor ebx, ebx
div bl ; Unhandled DIV0 exception

global jmp_usermode_test
jmp_usermode_test:
pop eax ; return address
mov ecx, esp ; return stack
call save_current_task
mov esp, 0x500000 ; usermode stack
mov eax, 0x20 | 3
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

extern save_current_task

global flushTSS
flushTSS:
mov ax, 0x28
ltr ax
ret

extern tss_data

global ivt
ivt: dd 0x00000000

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
