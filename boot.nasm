; This boots the active partition.
; Relocates self to 0x7E00, loads the 
; first sector of active partition
; to 0x7C00 and jumps
[ORG 0x7C00]
[BITS 16]
xor ax, ax
mov ds, ax
mov es, ax
mov ax, 0x8000
mov ss, ax
mov sp, 0xFF00
; Relocate self
mov di, 0x7E00
mov si, 0x7C00
mov cx, 512
rep movsw
jmp 0:relocated
; TODO Make this calculated, somehow
[SECTION RELOC vstart=0x7E20]
relocated:
mov ah, 0x41 ; int 13 extensions check
mov bx, 0x55AA
int 0x13
jc no_int13
; Find active partition
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
; Put partition start LBA in disk address packet
add si, 7
mov di, addr_packet_start_block
movsw
movsw
; Load the first sector of the partition
xor ax, ax
mov ah, 0x42
mov si, addr_packet
int 0x13
jc err
; Jump to partition boot
jmp 0:0x7C00

no_int13:
mov si, no_exten_str
mov cx, no_exten_str_end - no_exten_str
jmp print
err:
mov bx, ax
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
string: db 'DISK ERROR'
string_end:
no_exten_str: db 'NO INT13 EXTEN'
no_exten_str_end:

addr_packet:
db 0x10, 0x00 ; size, reserved
dw 1 ; blocks
addr_packet_transfer_buff_off: dw 0x7C00 ; transfer buffer offset
addr_packet_transfer_buff_seg: dw 0x0000 ; transfer buffer segment
addr_packet_start_block: dq 0 ; start block
