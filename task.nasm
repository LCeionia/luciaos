global flushTSS
flushTSS:
mov ax, 0x28
ltr ax
ret

task_ptr: equ (0x310000-4)

; return address in EAX
; return stack in ECX
; we can modify EAX, ECX, EDX
; i.e. save all others in task
global save_current_task
save_current_task:
push edx
mov edx, esp ; EDX holds our tmp stack, unsaved
mov esp, dword [task_ptr] ; load current task pointer
push ss
push ecx ; return stack
pushfd
push cs
push eax ; return address
push ds ; other segs, pop
push es ; before iret
push fs ; in exit handler
push gs
push ebp ; saved
push ebx ; saved
push esi ; saved
push edi ; saved
mov dword [task_ptr], esp ; save new task pointer
mov esp, edx
pop edx
ret

global return_prev_task
return_prev_task:
mov ecx, eax ; save return value for later
mov edx, dword [task_ptr] ; load current task pointer
add dword [task_ptr], 52 ; adjust to last task pointer
mov edi, [edx+0]
mov esi, [edx+4]
mov ebx, [edx+8]
mov ebp, [edx+12]
mov eax, [edx+0+16] ; gs
mov gs, ax
mov eax, [edx+4+16] ; fs
mov fs, ax
mov eax, [edx+8+16] ; es
mov es, ax
; SS:ESP <- return stack
mov esp, [edx+28+16] ; esp
mov eax, [edx+32+16] ; ss
mov ss, ax
mov eax, [edx+24+16] ; eflags
push eax
mov eax, [edx+20+16] ; cs
push eax
mov eax, [edx+16+16] ; eip
push eax
mov eax, [edx+12+16] ; ds
mov ds, ax
mov eax, ecx ; restore return value
iret

; extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
global enter_v86
enter_v86:
pop eax ; return address
mov ecx, esp ; return stack
call save_current_task
mov ebp, esp               ; save stack pointer
push dword  [ebp+0]        ; ss
push dword  [ebp+4]        ; esp
pushfd                     ; eflags
or dword [esp], (1 << 17)  ; set VM flags
;or dword [esp], (3 << 12) ; IOPL 3
push dword [ebp+8]        ; cs
push dword  [ebp+12]       ; eip
iret

; return address in eax, return stack in ebp
;extern save_current_task

extern user_test
global jmp_usermode_test
jmp_usermode_test:
pop eax ; return address
mov ecx, esp ; return stack
call save_current_task
mov esp, 0x800000 ; usermode stack
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
