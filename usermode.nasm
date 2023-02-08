[SECTION .user]
global user_test
user_test:
mov dword [0xb8000], 0x0f000f00 | 'U' | 's' << 16
mov dword [0xb8004], 0x0f000f00 | 'e' | 'r' << 16
mov dword [0xb8008], 0x0f000f00 | 'm' | 'o' << 16
mov dword [0xb800C], 0x0f000f00 | 'd' | 'e' << 16
mov dword [0xb8010], 0x0f000f00 | '!'
mov dword [0xb8000+160], 0x0f000f00 | 'F' | 'a' << 16
mov dword [0xb8004+160], 0x0f000f00 | 'u' | 'l' << 16
mov dword [0xb8008+160], 0x0f000f00 | 't' | 's' << 16
mov dword [0xb800C+160], 0x0f000f00 | ':' | ' ' << 16
mov dword [0xb8000+320], 0x0f000f00 | 'd' | ':' << 16
mov dword [0xb8004+320], 0x0f000f00 | ' ' | '#' << 16
mov dword [0xb8008+320], 0x0f000f00 | 'D' | 'I' << 16
mov dword [0xb800C+320], 0x0f000f00 | 'V' | ' ' << 16
mov dword [0xb8000+480], 0x0f000f00 | 'p' | ':' << 16
mov dword [0xb8004+480], 0x0f000f00 | ' ' | 'P' << 16
mov dword [0xb8008+480], 0x0f000f00 | 'G' | 'F' << 16
mov dword [0xb800C+480], 0x0f000f00 | 'L' | 'T' << 16
mov dword [0xb8000+640], 0x0f000f00 | 'u' | ':' << 16
mov dword [0xb8004+640], 0x0f000f00 | ' ' | '#' << 16
mov dword [0xb8008+640], 0x0f000f00 | 'U' | 'D' << 16
mov dword [0xb800C+640], 0x0f000f00 | '2' | ' ' << 16
mov dword [0xb8000+800], 0x0f000f00 | 'g' | ':' << 16
mov dword [0xb8004+800], 0x0f000f00 | ' ' | '#' << 16
mov dword [0xb8008+800], 0x0f000f00 | 'G' | 'P' << 16
mov dword [0xb800C+800], 0x0f000f00 | 'F' | ' ' << 16
mov eax, 0 ; command = 00h, get key
int 0x21 ; OS call
cmp al, 'd'
je .div_err
cmp al, 'p'
je .page_err
cmp al, 'u'
je .undefined_err
cmp al, 'g'
je .gpf_err
push 0x00000013 ; eax  AH=0,AL=3 set video mode 3
push 0x00000000 ; ecx
push 0x00000000 ; edx
push 0x00000000 ; ebx
push 0x00000000 ; esi
push 0x00000000 ; edi
push esp ; regs
push 0x10 ; interrupt
mov eax, 0x86 ; command = 86h, virtual 8086 call
int 0x21 ; OS call
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
.page_err:
mov edx, 0x105000 ; somewhere in kernel mem
mov edx, [edx] ; should page fault
.div_err:
xor ebx, ebx
div bl ; Unhandled DIV0 exception
.undefined_err:
ud2 ; undefined instruction
.gpf_err:
mov ax, 0x10
mov ds, ax   ; load ds with bad value
