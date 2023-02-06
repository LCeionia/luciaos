secreadcnt equ 0x38
kernelreads equ 0x10000/(512*secreadcnt) ; 64K / Sector Size FIXME This underestimates when kernel size is not divisible by bytes per read
[ORG 0x7c00]
[BITS 16]
xor ax, ax
mov ds, ax
mov es, ax
mov ax, 0x8000
mov ss, ax
mov sp, 0xFF00
mov cx, kernelreads
read_loop:
xor ax, ax
mov ah, 0x42
mov si, addr_packet
int 0x13
jc err
add word [addr_packet_transfer_buff_seg], (secreadcnt*512)/16
add word [addr_packet_start_block], secreadcnt
loop read_loop
entry:
cli             ; no interrupts
xor ax,ax
mov ds, ax
lgdt [gdt_desc]  ; load gdt register
mov eax, cr0    ; set pmode bit
or al, 1
mov cr0, eax
jmp 08h:Pmode
err:
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
[BITS 32]
Pmode:
mov eax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
in al, 0x92
test al, 2
jnz after
or al, 2
and al, 0xFE
out 0x92, al
after:
mov esi, 0x8000
mov edi, 0x100000
mov ecx, 0x10000
rep movsb
jmp 08h:0x100000
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
gdt_end:

string: db 'DISK ERROR'

addr_packet:
db 0x10, 0x00 ; size, reserved
dw secreadcnt ; blocks
addr_packet_transfer_buff_off: dw 0x0000 ; transfer buffer offset
addr_packet_transfer_buff_seg: dw 0x0800 ; transfer buffer segment
addr_packet_start_block: dq 1 ; start block
