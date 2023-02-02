[SECTION .user]
global user_test
user_test:
mov dword [0xb8000], 0x0f000f00 | 'U' | 's' << 16
mov dword [0xb8004], 0x0f000f00 | 'e' | 'r' << 16
mov dword [0xb8008], 0x0f000f00 | 'm' | 'o' << 16
mov dword [0xb800C], 0x0f000f00 | 'd' | 'e' << 16
mov word [0xb8010], 0x0f00 | '!'
mov eax, 0 ; command = 00, get key
int 0x21 ; OS call
push 0x00000013 ; eax  AH=0,AL=3 set video mode 3
push 0x00000000 ; ecx
push 0x00000000 ; edx
push 0x00000000 ; ebx
push 0x00000000 ; esi
push 0x00000000 ; edi
push esp ; regs
mov eax, 0x10 ; command = 10
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
mov edx, 0x105000 ; somewhere in kernel mem
mov edx, [edx] ; should page fault
xor ebx, ebx
div bl ; Unhandled DIV0 exception
