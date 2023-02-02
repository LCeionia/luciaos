#include <stdint.h>

#include "dosfs/dosfs.h"
#include "print.h"
#include "interrupt.h"
#include "tss.h"
#include "paging.h"

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

uint32_t get_flags() {
    uint32_t flags;
    asm volatile("pushfd\npop %%eax":"=a"(flags));
    return flags;
}
uint32_t get_cr0() {
    uint32_t reg;
    asm volatile("mov %%cr0, %%eax":"=a"(reg));
    return reg;
}
uint32_t get_cr3() {
    uint32_t reg;
    asm volatile("mov %%cr3, %%eax":"=a"(reg));
    return reg;
}
uint32_t get_cr4() {
    uint32_t reg;
    asm volatile("mov %%cr4, %%eax":"=a"(reg));
    return reg;
}

struct __attribute((__packed__)) Int13DiskPacket_t {
    uint8_t size; // 0x10
    uint8_t reserved; // 0x00
    uint16_t blocks;
    uint32_t transfer_buffer; // 0x2300:0000
    uint64_t start_block;
};

extern struct Int13DiskPacket_t v86disk_addr_packet;

extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
extern void v86Test();
extern void v86TransFlag();
extern void v86GfxMode();
extern void v86TextMode();
extern void v86DiskRead();
extern char *jmp_usermode_test();

extern char _edata, _v86code, _ev86code, _bstart, _bend, _loadusercode, _usercode, _eusercode;
void setup_binary() {
    // Put V86 code in proper place based on linker
    char *s = &_edata;
    char *d = &_v86code;
    while (d < &_ev86code)
        *d++ = *s++;

    // Put Usermode code in proper place based on linker
    s = &_loadusercode;
    d = &_usercode;
    while (d < &_eusercode)
        *d++ = *s++;

    // Clear BSS area
    for (d = &_bstart; d < &_bend; d++)
        *d = 0;
}

uint16_t error_screen[80*50]; // 50-line VGA screen of error content

extern uint16_t *ivt;
uint16_t ivt_bkup[0x200];
uint8_t bios_bkup[0x40000];
void backup_ivtbios() {
    for (int i = 0; i < 0x200; i++)
        ivt_bkup[i] = ivt[i];
    for (int i = 0; i < 0x40000; i++)
        bios_bkup[i] = ((uint8_t*)0xC0000)[i];
}

// This should only be used during fault handling, as it is expensive
void ensure_v86env() {
    for (int i = 0; i < 0x200; i++)
        ivt[i] = ivt_bkup[i];
    for (int i = 0; i < 0x40000; i++)
        ((uint8_t*)0xC0000)[i] = bios_bkup[i];
    char *s = &_edata;
    char *d = &_v86code;
    while (d < &_ev86code)
        *d++ = *s++;
    for (int i = 0; i < 80*50; i++)
        if (!(error_screen[i] & 0xFF00))
            error_screen[i] = 0x0f00 | (error_screen[i] & 0x00FF);
}

void error_environment() {
    ensure_v86env();
    FARPTR v86_entry = i386LinearToFp(v86TextMode);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
    printStr("Oh noes!!! System error! ;c    Press E for a fun recovery :3", &error_screen[80]);
    uint16_t *vga_text = ((uint16_t*)0xB8000);
    for (int i = 0; i < 80*50; i++)
        vga_text[i] = error_screen[i];
    uint8_t key;
    for (key = get_key(); key != 'e' && key != 'E'; key = get_key());
    v86_entry = i386LinearToFp(v86TransFlag);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
}

/*
Real Mode Accessible (First MB)
 00000 -  00400 IVT (1kB)
 00400 -  01000 Unused (3kB)
 01000 -  04000 Free (12kB)
 04000 -  07C00 Free (15kB)
 07C00 -  08000 Boot (512B)
 08000 -  20000 V86 Code (96kB)
 20000 -  30000 Disk Buffer (64kB)
 30000 -  80000 Free (320kB)
 80000 -  90000 Real Mode Stack (64kB)
 90000 -  A0000 Free (64kB)
 A0000 -  C0000 VGA (128kB)
 C0000 -  FFFFF BIOS Area (256kB)
Protected Only (1MB+)
100000 - 200000 Kernel Code (1mB)
200000 - 200080 TSS (128B)
200080 - 202080 TSS IOMAP (8kB)
202080 - 300000 Free (~1mB)
300000 - 310000 Task Stack (64kB)
310000 - 320000 Interrupt Stack (64kB)
320000 - 400000 Kernel Stack (896kB)
400000 - 700000 Usermode Code (3mB)
700000 - 800000 Usermode Stack (1mB)
*/

void TestV86() {
    FARPTR v86_entry = i386LinearToFp(v86Test);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
}
void TestUser() {
    char *vga = jmp_usermode_test();
    for (int i = 0; i < 320; i++) {
        vga[i] = i;
    }
}
void TestDiskRead() {
    word *vga_text = (word *)0xb8000 + (80*5);
    vga_text += printStr("Setting Text Mode... ", vga_text);
    FARPTR v86_entry = i386LinearToFp(v86TextMode);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
    vga_text += printStr("Done. Starting Disk Read... ", vga_text);
    v86_entry = i386LinearToFp(v86DiskRead);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry));
    vga_text = (word *)0xb8000;
    char *diskReadBuf = (char *)0x23000;
    for (int i = 0; i < (80*25)/2; i++) {
        printByte(diskReadBuf[i], &vga_text[i*2]);
    }
}
void TestFAT() {
    word *vga_text = (word *)0xb8000;
    uint8_t *diskReadBuf = (uint8_t *)0x22400;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    VOLINFO vi;

    uint8_t pactive, ptype;
    uint32_t pstart, psize;
    pstart = DFS_GetPtnStart(0, diskReadBuf, 0, &pactive, &ptype, &psize);
    vga_text = (word *)0xb8000;
    vga_text += printStr("PartStart: ", vga_text);
    vga_text += printDword(pstart, vga_text);
    vga_text += 2;
    vga_text += printStr("PartSize: ", vga_text);
    vga_text += printDword(psize, vga_text);
    vga_text += 2;
    vga_text += printStr("PartActive: ", vga_text);
    vga_text += printByte(pactive, vga_text);
    vga_text += 2;
    vga_text += printStr("PartType: ", vga_text);
    vga_text += printByte(ptype, vga_text);
    vga_text = (word *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
    //asm ("xchgw %bx, %bx");

    DFS_GetVolInfo(0, diskReadBuf, pstart, &vi);
    vga_text += printStr("Label: ", vga_text);
    vga_text += printStr((char*)vi.label, vga_text);
    vga_text += 2;
    vga_text += printStr("Sec/Clus: ", vga_text);
    vga_text += printByte(vi.secperclus, vga_text);
    vga_text += 2;
    vga_text += printStr("ResrvSec: ", vga_text);
    vga_text += printWord(vi.reservedsecs, vga_text);
    vga_text += 2;
    vga_text += printStr("NumSec: ", vga_text);
    vga_text += printDword(vi.numsecs, vga_text);
    vga_text += 2;
    vga_text += printStr("Sec/FAT: ", vga_text);
    vga_text += printDword(vi.secperfat, vga_text);
    vga_text += 2;
    vga_text += printStr("FAT1@: ", vga_text);
    vga_text += printDword(vi.fat1, vga_text);
    vga_text += 2;
    vga_text += printStr("ROOT@: ", vga_text);
    vga_text += printDword(vi.rootdir, vga_text);
    vga_text = (word *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
    //asm ("xchgw %bx, %bx");

    vga_text += printStr("Files in root:", vga_text);
    DIRINFO di;
    di.scratch = diskReadBuf;
    DFS_OpenDir(&vi, (uint8_t*)"", &di);
    vga_text = (word *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
    DIRENT de;
    while (!DFS_GetNext(&vi, &di, &de)) {
        if (de.name[0]) {
            for (int i = 0; i < 11 && de.name[i]; i++) {
                if (i == 8) { *(uint8_t*)vga_text = ' '; vga_text++; } // space for 8.3
                *(uint8_t *)vga_text = de.name[i];
                vga_text++;
            }
            vga_text += printStr("  ", vga_text);
            vga_text += printDec((uint32_t)de.filesize_0 + ((uint32_t)de.filesize_1 << 8) + ((uint32_t)de.filesize_2 << 16) + ((uint32_t)de.filesize_3 << 24), vga_text);
            *(uint8_t*)vga_text++ = 'B';
            vga_text = (word *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
        }
        //asm ("xchgw %bx, %bx");
    }
}

void start() {
    word *vga_text = (word *)0xb8000;
    char h[] = "LuciaOS";
    for (int i = 0; i < sizeof(h); i++)
        *(char *)&vga_text[i] = h[i];
    vga_text = &vga_text[80];

    uint32_t o;
    asm("mov %%esp, %%eax" : "=a"(o) : :);
    vga_text += printDword(o, vga_text);
    vga_text++;
    asm("mov %%ebp, %%eax" : "=a"(o) : :);
    vga_text += printDword(o, vga_text);
    vga_text++;

    //char c[] = "APIC support: ";
    //char apic;
    //printByte(apic = check_apic(), (short*)&vga_text[80*3 + sizeof(c) - 1]);
    //for (int i = 0; i < sizeof(c); i++)
    //    *(char *)&vga_text[i+(80*3)] = c[i];
    //if (!apic) return;

    char sse_str[] = "SSE support: ";
    vga_text += printStr("SSE: ", vga_text);
    char sse = check_sse();
    if (!sse) {
        *vga_text = 'N';
        return;
    }
    vga_text += printStr("Y ", vga_text);
    enable_sse();

    setup_binary();

    // edit
    setup_interrupts();
    setup_tss();
    init_paging();
    //print_flags();
    vga_text += printStr("CR0:", vga_text);
    vga_text += printDword(get_cr0(), vga_text);
    vga_text++;
    //print_cr3();
    //print_cr4();

    vga_text += printStr("V86 Test... ", vga_text);
    //asm ("xchgw %bx, %bx");
    TestV86(); // has int 3 wait in v86
    vga_text = (word *)0xb8000 + (80*3);
    backup_ivtbios();
    vga_text += printStr("Done. Press 'N' for next test.", vga_text);
    uint8_t key;
    while ((key = get_key()) != 'N') {
        *vga_text = (*vga_text & 0xFF00) | key;
        vga_text++;
    }
    TestUser();
    kbd_wait();
    TestDiskRead();
    kbd_wait();
    TestFAT();
    kbd_wait();

    vga_text = &((uint16_t*)0xB8000)[80*16];
    vga_text += printStr("Press ` for a flagrant system error... ", vga_text);
    while ((key = get_key()) != '`') {
        *vga_text = (*vga_text & 0xFF00) | key;
        vga_text++;
    }
    // flagrant system error
    *((uint8_t*)0x1000000) = 0;
}

