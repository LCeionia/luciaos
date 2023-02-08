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
mov ah, 0x41 ; int 13 extensions check
mov bx, 0x55AA
int 0x13
jc no_exten
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
mov ax,0x2403 ; A20 BIOS Support
int 0x15
jb .no_a20
cmp ah,0
jnz .no_a20
mov ax,0x2401 ; A20 BIOS Activate
int 0x15
.no_a20:
cli             ; no interrupts
xor ax,ax
mov ds, ax
lgdt [gdt_desc]  ; load gdt register
mov eax, cr0    ; set pmode bit
or al, 1
mov cr0, eax
jmp 08h:Pmode
err:
mov bx, ax
mov si, string
mov cx, string_end - string
jmp print
no_exten:
mov bx, ax
mov si, no_exten_str
mov cx, no_exten_str_end - no_exten_str
print:
push 0xb800
pop es
xor di, di
mov ah, 0x7
err_print:
lodsb
stosw
loop err_print
add di, 2
mov ax, bx
call hexprint
hlt_loop:
hlt
jmp hlt_loop
hexprint:
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
mov ah, 0x7
stosw
test cx, cx
jz .nibble2
ret

[BITS 32]
Pmode:
mov eax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
; check A20
.a20:
mov edi, 0x101337
mov esi, 0x001337
mov [esi], esi
mov [edi], edi
cmpsd
jne .kernel
in al, 0x92 ; fast A20
test al, 2
jnz .fa20_end
or al, 2
and al, 0xFE
out 0x92, al
.fa20_end:
jmp .a20
.kernel:
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
string_end:
no_exten_str: db 'NO INT13 EXTEN'
no_exten_str_end:

addr_packet:
db 0x10, 0x00 ; size, reserved
dw secreadcnt ; blocks
addr_packet_transfer_buff_off: dw 0x0000 ; transfer buffer offset
addr_packet_transfer_buff_seg: dw 0x0800 ; transfer buffer segment
addr_packet_start_block: dq 1 ; start block
