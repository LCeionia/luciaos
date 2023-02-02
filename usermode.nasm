[SECTION .user]
global user_test
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
mov edx, 0x105000 ; somewhere in kernel mem
mov edx, [edx] ; should page fault
xor ebx, ebx
div bl ; Unhandled DIV0 exception
