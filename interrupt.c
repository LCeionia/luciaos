#include <stdint.h>
#include <stddef.h>

#include "print.h"

#include "interrupt.h"

struct __attribute__((__packed__)) IDTR_t {
    uint16_t size;
    uint32_t offset;
};

struct __attribute__((__packed__)) InterruptDescriptor32 {
   uint16_t offset_1;        // offset bits 0..15
   uint16_t selector;        // a code segment selector in GDT or LDT
   uint8_t  zero;            // unused, set to 0
   uint8_t  type_attributes; // gate type, dpl, and p fields
   uint16_t offset_2;        // offset bits 16..31
};

__attribute__((aligned(0x10)))
struct InterruptDescriptor32 IDT[256];
struct IDTR_t IDTR;

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %%al, %%dx" : : "d"(port), "a"(value));
}
uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %%dx, %%al" : "=a"(value) : "d"(port));
    return value;
}

FARPTR i386LinearToFp(void *ptr)
{
    unsigned seg, off;
    off = (uintptr_t) ptr & 0xffff;
    seg = ((uintptr_t) ptr >> 16);
    return MK_FP(seg, off);
}


void IRQ_set_mask(char IRQline) {
    uint16_t port;
    uint8_t value;
    if (IRQline < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);
}
void IRQ_clear_mask(char IRQline) {
    uint16_t port;
    uint8_t value;
    if (IRQline < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);
}

char v86_if = 0;
extern void real_test();
extern void kbd_wait();
#define VALID_FLAGS 0xDFF
__attribute__ ((interrupt))
void gpf_handler_v86(struct interrupt_frame *frame, unsigned long error_code) {
    asm volatile("mov %%ax,%%ds"::"a"(0x10));
    uint8_t *ip;
    uint16_t *stack, *ivt;
    uint32_t *stack32;
    char is_operand32 = 0, is_address32 = 0;
    ip = (size_t)(((frame->cs << 4) + frame->eip) & 0xFFFFF);
    ivt = (uint16_t*)0x0000;
    stack = FP_TO_LINEAR(frame->ss, frame->esp);
    stack32 = (uint32_t*)stack;

    char *vga = (char*)0xb8000 + (160 * 10);
    vga[0] = 'I'; vga[2] = 'P'; printWord(frame->eip, &vga[4]); vga += 14;
    vga[0] = 'C'; vga[2] = 'S'; printWord(frame->cs, &vga[4]); vga += 14;
    vga[0] = 'F'; vga[2] = 'L'; printDword(frame->eflags, &vga[4]); vga += 14;
    vga = (char*)0xb8000 + (160 * 11);
    vga[0] = 'S'; vga[2] = 'P'; printWord(frame->esp, &vga[4]); vga += 14;
    vga[0] = 'S'; vga[2] = 'S'; printWord(frame->ss, &vga[4]); vga += 14;
    vga = (char*)0xb8000 + (160 * 12);
    vga[0] = 'E'; vga[2] = 'S'; printWord(frame->es, &vga[4]); vga += 14;
    vga[0] = 'D'; vga[2] = 'S'; printWord(frame->ds, &vga[4]); vga += 14;
    vga[0] = 'F'; vga[2] = 'S'; printWord(frame->fs, &vga[4]); vga += 14;
    vga[0] = 'G'; vga[2] = 'S'; printWord(frame->gs, &vga[4]); vga += 14;

    //vga[2]++;
    //printDword(frame, &vga[20]);
    //vga = &vga[38];
    //uint32_t *fr = frame;
    //for (int i = 0; i < sizeof(struct interrupt_frame)/sizeof(uint32_t); i++) {
    //    printDword(fr[i], vga);
    //    vga += (sizeof(uint32_t)*2+1)*2;
    //}
    //vga = (char*)0xb80A0;
    //printDword(ip, &vga[20]);
    //vga = &vga[38];
    //for (int i = 0; i < 16; i++) {
    //    printByte(ip[i], vga);
    //    vga += (sizeof(uint8_t)*2)*2;
    //}
    vga = (char*)0xb8000 + (160*3);
    for(;;) {
        switch (ip[0]) {
            case 0x66: // O32
                is_operand32 = 1;
                ip++;
                frame->eip = (uint16_t)(frame->eip + 1);
                break;
            case 0x67: // A32
                is_address32 = 1;
                ip++;
                frame->eip = (uint16_t)(frame->eip + 1);
                break;
            case 0x9C: // PUSHF
                if (is_operand32) {
                    frame->esp = ((frame->esp & 0xffff) - 4) & 0xffff;
                    stack32--;
                    stack32[0] = frame->eflags & VALID_FLAGS;
                    if (v86_if)
                        stack32[0] |= EFLAG_IF;
                    else 
                        stack32[0] &= ~EFLAG_IF;
                } else {
                    frame->esp = ((frame->esp & 0xffff) - 2) & 0xffff;
                    stack--;
                    stack[0] = (uint16_t)frame->eflags;
                    if (v86_if)
                        stack[0] |= EFLAG_IF;
                    else 
                        stack[0] &= ~EFLAG_IF;
                }
                frame->eip = (uint16_t)(frame->eip + 1);
                goto done;
            case 0x9D: // POPF
                if (is_operand32) {
                    frame->eflags = EFLAG_IF | EFLAG_VM | (stack32[0] & VALID_FLAGS);
                    v86_if = (stack32[0] & EFLAG_IF) != 0;
                    frame->esp = ((frame->esp & 0xffff) + 4) & 0xffff;
                } else {
                    frame->eflags = EFLAG_IF | EFLAG_VM | stack[0];
                    v86_if = (stack[0] & EFLAG_IF) != 0;
                    frame->esp = ((frame->esp & 0xffff) + 2) & 0xffff;
                }
                frame->eip = (uint16_t)(frame->eip + 1);
                goto done;
            case 0xCD: // INT n
                vga[0] = 'I'; vga[2]++; if (vga[2] < '0') vga[2] = '0';
                asm volatile("nop\nnop");
                switch (ip[1]) {
                    case 0x30: // Exit V86 Mode?
                        asm volatile("nop\nnop");
                        asm volatile("jmp jmp_usermode_test");
                        //__builtin_unreachable();
                        break;
                    //case 0x20: // ???
                    //case 0x21: // ???
                    case 0x3:  // Debugger trap
                        kbd_wait();
                        asm volatile("nop");
                    default:
                        stack -= 3;
                        frame->esp = ((frame->esp & 0xffff) - 6) & 0xffff;

                        stack[0] = (uint16_t) (frame->eip + 2);
                        stack[1] = frame->cs;
                        stack[2] = (uint16_t) frame->eflags;
                        
                        if (v86_if)
                            stack[2] |= EFLAG_IF;
                        else
                            stack[2] &= ~EFLAG_IF;

                        frame->cs = ivt[ip[1] * 2 + 1];
                        frame->eip = ivt[ip[1] * 2];
                        asm volatile("nop");
                        //frame->cs = 0;
                        //frame->eip = real_test;
                        goto done;
                }
                goto done;
            case 0xCF: // IRET
                frame->eip = stack[0];
                frame->cs = stack[1];
                frame->eflags = EFLAG_IF | EFLAG_VM | stack[2];
                v86_if = (stack[2] & EFLAG_IF) != 0;
                frame->esp = ((frame->esp & 0xffff) + 6) & 0xffff;
                goto done;
            case 0xFA: // CLI
                v86_if = 0;
                frame->eip = (uint16_t) (frame->eip + 1);
                goto done;
            case 0xFB: // STI
                v86_if = 1;
                frame->eip = (uint16_t) (frame->eip + 1);
                goto done;
            default:
                for(;;);
                goto done;
        }
    }
    done:
    vga = (char*)0xb8000 + (160 * 13);
    vga[0] = 'I'; vga[2] = 'P'; printWord(frame->eip, &vga[4]); vga += 14;
    vga[0] = 'C'; vga[2] = 'S'; printWord(frame->cs, &vga[4]); vga += 14;
    vga[0] = 'F'; vga[2] = 'L'; printDword(frame->eflags, &vga[4]); vga += 14;
    vga = (char*)0xb8000 + (160 * 14);
    vga[0] = 'S'; vga[2] = 'P'; printWord(frame->esp, &vga[4]); vga += 14;
    vga[0] = 'S'; vga[2] = 'S'; printWord(frame->ss, &vga[4]); vga += 14;
    vga = (char*)0xb8000 + (160 * 15);
    vga[0] = 'E'; vga[2] = 'S'; printWord(frame->es, &vga[4]); vga += 14;
    vga[0] = 'D'; vga[2] = 'S'; printWord(frame->ds, &vga[4]); vga += 14;
    vga[0] = 'F'; vga[2] = 'S'; printWord(frame->fs, &vga[4]); vga += 14;
    vga[0] = 'G'; vga[2] = 'S'; printWord(frame->gs, &vga[4]); vga += 14;
}

extern void timerHandler();
extern void keyboardHandler();
extern void gpfHandler();
extern void unhandled_handler();
extern void picInit();
void set_system_gate(uint8_t gate, void (*handler)()) {
    IDT[gate].offset_1 = (uint32_t)(size_t)handler & 0xFFFF;
    IDT[gate].offset_2 = ((uint32_t)(size_t)handler >> 16) & 0xFFFF;
    IDT[gate].selector = 0x08;
    IDT[gate].type_attributes = 0x8E;
}
void set_trap_gate(uint8_t gate, void (*handler)()) {
    IDT[gate].offset_1 = (uint32_t)(size_t)handler & 0xFFFF;
    IDT[gate].offset_2 = ((uint32_t)(size_t)handler >> 16) & 0xFFFF;
    IDT[gate].selector = 0x08;
    IDT[gate].type_attributes = 0x8F;
}
void setup_interrupts() {
    IDTR.size = 256*8 - 1;
    IDTR.offset = (uint32_t)(size_t)IDT;
    for (int i = 0; i < 256; i++) {
        *(uint64_t*)&IDT[i] = 0;
    }

    for (int i = 0; i < 9; i++) {
        set_trap_gate(i, unhandled_handler);
    }
    for (int i = 10; i < 15; i++) {
        set_trap_gate(i, unhandled_handler);
    }
    for (int i = 16; i < 22; i++) {
        set_trap_gate(i, unhandled_handler);
    }

    set_system_gate(0x20, timerHandler);
    set_system_gate(0x21, keyboardHandler);
    set_trap_gate(13, gpf_handler_v86);
    //set_trap_gate(13, gpfHandler);

    asm volatile("lidt %0": : "m"(IDTR));
    picInit();
    IRQ_clear_mask(0);
    IRQ_clear_mask(1);
    asm volatile("sti");
}

