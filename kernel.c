#include <stdint.h>

typedef unsigned short word;

char nibbleToHex(char n) {
    return n > 9 ? (n - 10) + 'A' : n + '0';
}
void printByte(char v, short *buff) {
    *(char *)&buff[0] = nibbleToHex((v >> 4) & 0xF);
    *(char *)&buff[1] = nibbleToHex(v & 0xF);
}
void printWord(short v, short *buff) {
    printByte(v >> 8, buff);
    printByte(v, &buff[2]);
}
void printDword(int v, short *buff) {
    printWord(v >> 16, buff);
    printWord(v, &buff[4]);
}

char check_apic() {
    uint32_t eax, ebx, ecx, edx;
    asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx & (1 << 9)) != 0;
}
char check_sse() {
    uint32_t eax, ebx, ecx, edx;
    asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx & (1 << 25)) != 0;
}

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

struct interrupt_frame {
    uint32_t eip, cs;
    uint32_t eflags;
    uint32_t esp, ss;
    uint32_t es, ds, fs, gs;
};

/* Real Mode helper macros */
/* segment:offset pair */
typedef uint32_t FARPTR;

/* Make a FARPTR from a segment and an offset */
#define MK_FP(seg, off)  ((FARPTR) (((uint32_t) (seg) << 16) | (uint16_t) (off)))

/* Extract the segment part of a FARPTR */
#define FP_SEG(fp)        (((FARPTR) fp) >> 16)

/* Extract the offset part of a FARPTR */
#define FP_OFF(fp)        (((FARPTR) fp) & 0xffff)

/* Convert a segment:offset pair to a linear address */
#define FP_TO_LINEAR(seg, off) ((void*)(uintptr_t)((((uint32_t)seg) << 4) + ((uint32_t)off)))

FARPTR i386LinearToFp(void *ptr)
{
    unsigned seg, off;
    off = (uintptr_t) ptr & 0xffff;
    seg = ((uintptr_t) ptr >> 16);
    return MK_FP(seg, off);
}

#define EFLAG_IF ((uint32_t)1 << 9)
#define EFLAG_VM ((uint32_t)1 << 17)

char v86_if = 0;
extern void real_test();
extern void kbd_wait();
#define VALID_FLAGS 0xDFF
__attribute__ ((interrupt))
void gpf_handler_v86(struct interrupt_frame *frame, uint32_t error_code) {
    asm volatile("mov %%ax,%%ds"::"a"(0x10));
    uint8_t *ip;
    uint16_t *stack, *ivt;
    uint32_t *stack32;
    char is_operand32 = 0, is_address32 = 0;
    ip = ((frame->cs << 4) + frame->eip) & 0xFFFFF;
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
    IDT[gate].offset_1 = (uint32_t)handler & 0xFFFF;
    IDT[gate].offset_2 = ((uint32_t)handler >> 16) & 0xFFFF;
    IDT[gate].selector = 0x08;
    IDT[gate].type_attributes = 0x8E;
}
void set_trap_gate(uint8_t gate, void (*handler)()) {
    IDT[gate].offset_1 = (uint32_t)handler & 0xFFFF;
    IDT[gate].offset_2 = ((uint32_t)handler >> 16) & 0xFFFF;
    IDT[gate].selector = 0x08;
    IDT[gate].type_attributes = 0x8F;
}
void setup_interrupts() {
    IDTR.size = 256*8 - 1;
    IDTR.offset = (uint32_t)IDT;
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


struct __attribute__((__packed__)) tss_entry_struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trap;
    uint32_t iomap_base;
};
struct tss_entry_struct *tss_data;
void write_tss() {
    tss_data = (struct tss_entry_struct *)0x20000;
    for (int i = 0; i < 0x2080; i++)
        ((uint8_t*)tss_data)[i] = 0;
    tss_data->ss0 = 0x10;
    tss_data->esp0 = 0x400000;
    tss_data->iomap_base = 0x80;
}

void enable_sse() {
    asm volatile(
        "mov %%cr0, %%eax\n"
        "and $0xFFFB, %%ax\n"
        "or $0x2, %%ax\n"
        "mov %%eax, %%cr0\n"
        "mov %%cr4, %%eax\n"
        "or $0x600, %%ax\n"
        "mov %%eax, %%cr4\n"
        : : : "%eax"
    );
}

void print_flags() {
    uint32_t flags;
    asm volatile("pushfd\npop %%eax":"=a"(flags));
    printDword(flags, 0xB8000 + (160*4) + 50);
}
void print_cr0() {
    uint32_t reg;
    asm volatile("mov %%cr0, %%eax":"=a"(reg));
    printDword(reg, 0xB8000 + (160*5) + 50);
}
void print_cr3() {
    uint32_t reg;
    asm volatile("mov %%cr3, %%eax":"=a"(reg));
    printDword(reg, 0xB8000 + (160*5) + 50 + 8*2 + 2);
}
void print_cr4() {
    uint32_t reg;
    asm volatile("mov %%cr4, %%eax":"=a"(reg));
    printDword(reg, 0xB8000 + (160*5) + 50 + 8*4 + 4);
}

extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
extern void v86Code();
extern void flushTSS();
extern void jmp_usermode_test();

/*
Real Mode Accessible (First MB)
 00000 -  02000 IVT
 01000 -  04000 Paging
 04000 -  07C00 Free
 07C00 -  08000 Boot
 08000 -  20000 Kernel Code
 20000 -  22080 TSS
 80000 -  90000 Real Mode Stack
 90000 -  A0000 Free
 A0000 -  FFFFF BIOS Area
Protected Only (1MB+)
100000 -        Free
*/

void start() {
    word *vga_text = (word *)0xb8000;
    char h[] = "LuciaOS";
    for (int i = 0; i < sizeof(h); i++)
        *(char *)&vga_text[i] = h[i];

    uint32_t o;
    asm("mov %%esp, %%eax" : "=a"(o) : :);
    printDword(o, (short *)&vga_text[80]);
    asm("mov %%ebp, %%eax" : "=a"(o) : :);
    printDword(o, (short *)&vga_text[160]);

    //char c[] = "APIC support: ";
    //char apic;
    //printByte(apic = check_apic(), (short*)&vga_text[80*3 + sizeof(c) - 1]);
    //for (int i = 0; i < sizeof(c); i++)
    //    *(char *)&vga_text[i+(80*3)] = c[i];
    //if (!apic) return;

    char sse_str[] = "SSE support: ";
    char sse;
    printByte(sse = check_sse(), (short*)&vga_text[80*4 + sizeof(sse_str) - 1]);
    for (int i = 0; i < sizeof(sse_str); i++)
        *(char *)&vga_text[i+(80*4)] = sse_str[i];
    if (!sse) return;
    enable_sse();

    setup_interrupts();
    write_tss();
    flushTSS();
    print_flags();
    print_cr0();
    print_cr3();
    print_cr4();
    FARPTR v86_entry = i386LinearToFp(v86Code);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
    //jmp_usermode_test();
}

