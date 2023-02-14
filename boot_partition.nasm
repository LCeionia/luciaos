; This boots *from a FAT partition*
; So it starts at 0x7C00+90, the offset of
; the boot code in a FAT VBR
secreadcnt equ 0x38
kernelreads equ 0x10000/(512*secreadcnt) ; 64K / Sector Size FIXME This underestimates when kernel size is not divisible by bytes per read
[ORG 0x7c00+90]
[BITS 16]
xor ax, ax
mov ds, ax
mov es, ax
mov ax, 0x8000
mov ss, ax
mov sp, 0xFF00
; FIXME Assumes INT 13 Extension support
mov ah, 0x42
mov si, addr_packet
int 0x13
jc err
xor cx, cx
mov si, 0x7E00+0x1BE
.find_active:
lodsb
bt ax, 7
jc read
add si, 15
inc cl
cmp cl, 4
jl .find_active
jmp err
read:
mov dh, cl ; Store active partition
add si, 7
mov di, addr_packet_start_block
lodsw
; Offset of kernel in partition
add ax, 4
stosw
lodsw
adc ax, 0
stosw
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
mov si, string
mov cx, string_end - string
print:
push 0xb800
pop es
xor di, di
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
; check A20
.a20:
mov edi, 0x107C00
mov esi, 0x007C00
mov [esi], esi
mov [edi], edi
cmpsd
jne .kernel
; FIXME It's annoying that fast A20
; will crash old systems, but it's
; just so small...
in al, 0x92 ; fast A20
test al, 2
jnz .fa20_end
or al, 2
and al, 0xFE
out 0x92, al
.fa20_end:
jmp .a20
.kernel:
mov esi, 0x7E00
mov edi, 0x100000
mov ecx, 0x10000
rep movsb
xchg bx,bx
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

addr_packet:
db 0x10, 0x00 ; size, reserved
dw secreadcnt ; blocks
addr_packet_transfer_buff_off: dw 0x7E00 ; transfer buffer offset
addr_packet_transfer_buff_seg: dw 0x0000 ; transfer buffer segment
addr_packet_start_block: dq 0 ; start block
