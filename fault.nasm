extern error_screen ; 80x50 words 

; Switches to text mode and shit
extern error_environment
_fault_coda:
xchg bx,bx
mov ax, 0x10
mov es, ax
; move to 'safe' location
mov ebp, 0x318000
; copy stack a bit
mov ecx, 0
.copy:
mov eax, [esp+ecx*4]
mov [ebp+ecx*4], eax
inc ecx
cmp ecx, 6
jl .copy
mov esp, ebp
call error_environment
.hlt:
hlt
jmp .hlt

global _gpf_eax_save
_gpf_eax_save: dd 0
global _gpf_eflags_save
_gpf_eflags_save: dd 0
extern gpf_handler_v86
global gpfHandler
gpfHandler:
cli ; make sure we're in a 'friendly' env
push eax
push ebx
push ecx
; save old ds
mov bx, ds
mov ax, 0x10
mov ds, ax
mov word [_gpf_old_ds], bx
; relocate stack so other interrupts don't fuck us over
; not sure if this is necessary, it doesn't seem to fix our race conditions...
mov ebx, esp
sub esp, 0x1000
xor ecx, ecx
.l:
mov eax, [ebx]
mov [esp+ecx], eax
add ebx, 4
add ecx, 4
cmp ebx, 0x320000 ; tss esp0
jl .l
pop ecx
pop ebx
sti ; we shouldn't crash now?
mov eax, dword [esp+16] ; EFLAGS
mov dword [_gpf_eflags_save], eax ; save
and eax, 1 << 17 ; VM flag
test eax, eax
pop eax
mov dword [_gpf_eax_save], eax
jnz gpf_handler_v86
jmp gpf_handler_32
gpf_unhandled:
mov dword [error_screen+0x00], 0x0f000f00 | 'G' | 'P' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'F' | '!' << 16
xchg bx,bx
jmp _fault_coda

_gpf_old_ds: dw 0
extern get_key
extern get_scancode
extern task_ptr
extern _enter_v86_internal_no_task
extern return_prev_task
extern v86Interrupt
gpf_handler_32:
push eax
mov eax, dword [esp+8] ; EIP
movzx eax, word [eax]
cmp eax, 0x30CD ; int 0x30
je .int30
cmp eax, 0x21CD ; int 0x21
je .int21
jmp gpf_unhandled

.int21:
pop eax ; command
cmp al, 0x00 ; get key
jne .s1
call get_key
jmp .return_to_offender
.s1: cmp al, 0x01 ; get scancode
jne .s86
call get_scancode
jmp .return_to_offender
.s86: cmp al, 0x86 ; v86 interrupt call
je .v86int
cmp ax, 0x86D8 ; get v86 data pointer
jne .return_to_offender
mov eax, 0x30000 ; V86 Data
jmp .return_to_offender

.v86int:
add esp, 4
add dword [esp+0], 2
; add a new task
call _gpf_create_return_task
; now enter v86 mode
; get int & regs from return stack
mov eax, [esp+12] ; return esp
mov eax, [eax] ; interrupt
and eax, 0xff ; ensure 1 byte
shl eax, 8
or eax, 0x30CD00CD ; command
mov dword [v86Interrupt], eax
mov eax, [esp+12] ; return esp
mov eax, [eax+4] ; regs
push eax ; regs
mov eax, v86Interrupt
and eax, 0xffff
push eax ; ip
mov eax, v86Interrupt
shr eax, 4
and eax, 0xf000
push eax ; cs
push 0xFF00 ; sp
push 0x8000 ; ss
jmp _enter_v86_internal_no_task ; NOT a function call

.int30:
pop eax ; return value
jmp return_prev_task
.return_to_offender:
add dword [esp+4], 2
push eax
mov ax, word [_gpf_old_ds]
mov ds, ax
pop eax
add esp, 4 ; error code
iret

_gpf_create_return_task:
; handler stack stored in edx
mov edx, esp
mov esp, dword [task_ptr]
mov eax, [edx+20] ; ss
push eax
mov eax, [edx+16] ; esp
push eax
mov eax, [edx+12] ; eflags
push eax
mov eax, [edx+8] ; cs
push eax
mov eax, [edx+4] ; eip
push eax
mov ax, word [_gpf_old_ds] ; restore old ds
mov ds, ax
push ds
push es
push fs
push gs
push ebp
push ebx
push esi
push edi
mov ax, 0x10
mov ds, ax
mov dword [task_ptr], esp ; save new task pointer
mov esp, edx ; restore handler stack
ret

global unhandled_handler
unhandled_handler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | 'E' | 'R' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'R' | 'O' << 16
mov dword [error_screen+0x08], 0x0f000f00 | 'R' | '!' << 16
jmp _fault_coda

global pageFaultHandler
pageFaultHandler:
mov ax, 0x10
mov ds, ax
pop eax ; error code
mov ebx, 0x0f000f00 | '0' | '!' << 16
and eax, 0x7 ; U/S,R/W,P
add ebx, eax
mov dword [error_screen+0x00], 0x0f000f00 | 'P' | 'G' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'F' | 'L' << 16
mov dword [error_screen+0x08], 0x0f000f00 | 'T' | ':' << 16
mov dword [error_screen+0x0C], ebx
jmp _fault_coda


global divisionErrorHandler
divisionErrorHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'D' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'E' | '!' << 16
jmp _fault_coda
global boundRangeHandler
boundRangeHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'B' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'R' | '!' << 16
jmp _fault_coda
global invalidOpcodeHandler
invalidOpcodeHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'U' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'D' | '!' << 16
jmp _fault_coda
global deviceNotAvailableHandler
deviceNotAvailableHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'N' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'D' | '!' << 16
jmp _fault_coda
global doubleFaultHandler
doubleFaultHandler:
mov ax, 0x10
mov ds, ax
mov dword [0xb8000+0x00], 0x0f000f00 | '#' | 'D' << 16
mov dword [0xb8000+0x04], 0x0f000f00 | 'F' | '!' << 16
; double faults simply abort right then
.hlt: hlt
jmp .hlt
global invalidTSSHandler
invalidTSSHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'T' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'S' | '!' << 16
jmp _fault_coda
global segmentNotPresentHandler
segmentNotPresentHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'N' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'P' | '!' << 16
jmp _fault_coda
global stackSegmentHandler
stackSegmentHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'S' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'S' | '!' << 16
jmp _fault_coda
global x87FloatingHandler
x87FloatingHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'M' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'F' | '!' << 16
jmp _fault_coda
global alignmentCheckHandler
alignmentCheckHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'A' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'C' | '!' << 16
jmp _fault_coda
global controlProtectionHandler
controlProtectionHandler:
mov ax, 0x10
mov ds, ax
mov dword [error_screen+0x00], 0x0f000f00 | '#' | 'C' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'P' | '!' << 16
jmp _fault_coda
