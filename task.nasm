task_ptr: equ (0x310000-4)

global save_current_task
save_current_task:
push ebx
mov ebx, esp
mov esp, dword [task_ptr] ; load current task pointer
push ss
push ebp ; return stack
pushfd
push cs
push eax ; return address
push ds ; other segs, pop
push es ; before iret
push fs ; in exit handler
push gs
mov dword [task_ptr], esp ; save new task pointer
mov esp, ebx
pop ebx
ret

global return_prev_task
return_prev_task:
mov edi, eax ; save for later
mov esi, dword [task_ptr] ; load current task pointer
add dword [task_ptr], 36 ; adjust to last task pointer
mov eax, [esi+0] ; gs
mov gs, ax
mov eax, [esi+4] ; fs
mov fs, ax
mov eax, [esi+8] ; es
mov es, ax
mov ebx, [esi+16] ; eip
mov ecx, [esi+20] ; cs
mov edx, [esi+24] ; eflags
; SS:ESP <- return stack
mov esp, [esi+28] ; esp
mov eax, [esi+32] ; ss
mov ss, ax
mov eax, [esi+12] ; ds
mov ds, ax
push edx ; eflags
push ecx ; cs
push ebx ; eip
mov eax, edi ; restore return value
iret
