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
