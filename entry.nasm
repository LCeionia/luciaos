global entry
entry:
mov dword [0xb8000], 0x07000700 | 'E' | 'N' << 16
mov dword [0xb8004], 0x07000700 | 'T' | 'R' << 16
mov dword [0xb8008], 0x07000700 | 'Y' | ' ' << 16
lgdt [gdt_desc]  ; load gdt register
jmp 08h:Pmodecode

extern gdt_desc

[BITS 32]
Pmodecode:
mov ax, 0x10 ; set up segments
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov gs, ax
mov ebp, 0x800000
mov esp, ebp
mov eax, 0x1f001f00
mov ecx, (80*25)/2
mov edi, 0xb8000
rep stosd ; clear screen
mov eax, dword [kernel_check]
cmp eax, 0x12345678
jne err
call start
hlt_loop:
hlt
jmp hlt_loop
err:
mov dword [0xb8000], 0x07000700 | 'L' | 'O' << 16
mov dword [0xb8004], 0x07000700 | 'A' | 'D' << 16
mov dword [0xb8008], 0x07000700 | 'F' | 'A' << 16
mov dword [0xb800C], 0x07000700 | 'I' | 'L' << 16
jmp hlt_loop

extern start
extern kernel_check
