global entry
entry:
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
