#include <stdint.h>

#include "print.h"
#include "interrupt.h"

#include "tss.c"

typedef unsigned short word;

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
    printDword(o, (uint16_t *)&vga_text[80]);
    asm("mov %%ebp, %%eax" : "=a"(o) : :);
    printDword(o, (uint16_t *)&vga_text[160]);

    //char c[] = "APIC support: ";
    //char apic;
    //printByte(apic = check_apic(), (short*)&vga_text[80*3 + sizeof(c) - 1]);
    //for (int i = 0; i < sizeof(c); i++)
    //    *(char *)&vga_text[i+(80*3)] = c[i];
    //if (!apic) return;

    char sse_str[] = "SSE support: ";
    char sse;
    printByte(sse = check_sse(), (uint16_t*)&vga_text[80*4 + sizeof(sse_str) - 1]);
    for (int i = 0; i < sizeof(sse_str); i++)
        *(char *)&vga_text[i+(80*4)] = sse_str[i];
    if (!sse) return;
    enable_sse();

    setup_interrupts();
    setup_tss();
    print_flags();
    print_cr0();
    print_cr3();
    print_cr4();
    FARPTR v86_entry = i386LinearToFp(v86Code);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
    //jmp_usermode_test();
}

