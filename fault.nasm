extern error_screen ; 80x50 words 

; Switches to text mode and shit
extern error_environment
_fault_coda:
xchg bx,bx
mov ax, 0x10
mov es, ax
; move to TOP OF kernel stack
mov ebp, 0x400000
mov esp, ebp
call error_environment
.hlt:
hlt
jmp .hlt

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

extern gpf_handler_v86
global gpfHandler
gpfHandler:
push eax
mov ax, 0x10
mov ds, ax
mov eax, dword [esp+16] ; EFLAGS
and eax, 1 << 17 ; VM flag
test eax, eax
pop eax
jnz gpf_handler_v86
jmp gpf_handler_32
gpf_unhandled:
mov dword [error_screen+0x00], 0x0f000f00 | 'G' | 'P' << 16
mov dword [error_screen+0x04], 0x0f000f00 | 'F' | '!' << 16
jmp _fault_coda

gpf_handler_32:
push eax
mov eax, dword [esp+8] ; EIP
movzx eax, word [eax]
cmp eax, 0x30CD ; int 0x30
jne gpf_unhandled
pop eax ; return value
jmp return_prev_task

extern return_prev_task

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
