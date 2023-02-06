#include <stdint.h>

#include "dosfs/dosfs.h"
#include "print.h"
#include "interrupt.h"
#include "kbd.h"
#include "tss.h"
#include "paging.h"
#include "v86defs.h"
#include "tests.h"
#include "progs.h"
#include "helper.h"

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

extern char _loadusercode, _usercode, _eusercode;
void LoadUser() {
    // Put Usermode code in proper place based on linker
    char *s = &_loadusercode;
    char *d = &_usercode;
    while (d < &_eusercode)
        *d++ = *s++;
}

extern char _edata, _v86code, _ev86code, _bstart, _bend;
void setup_binary() {
    // Put V86 code in proper place based on linker
    char *s = &_edata;
    char *d = &_v86code;
    while (d < &_ev86code)
        *d++ = *s++;

    LoadUser();

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

__attribute((__no_caller_saved_registers__))
extern void return_prev_task();
void error_environment() {
    ensure_v86env();
    union V86Regs_t regs;
    FARPTR v86_entry = i386LinearToFp(v86TextMode);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
    char str0[] = "Oh noes!!! System error! ;c    ";
    printStr(str0, &error_screen[80]);
    printStr("Press E for a fun recovery :3", &error_screen[80+sizeof(str0)]);
    printStr("Press R to return to previous task", &error_screen[160+sizeof(str0)]);
    uint16_t *vga_text = ((uint16_t*)0xB8000);
    for (int i = 0; i < 80*50; i++)
        vga_text[i] = error_screen[i];
    for(;;) {
        uint8_t key = get_scancode() & 0xff;
        if (key == KEY_E) break;
        if (key == KEY_R) return_prev_task();
    }
    v86_entry = i386LinearToFp(v86TransFlag);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
}

/*
Real Mode Accessible (First MB)
 00000 -  00400 IVT (1kB)
 00400 -  01000 Unused (3kB)
 01000 -  04000 Free (12kB)
 04000 -  07C00 V86 Code (15kB)
 07C00 -  08000 Boot & V86 Code (512B)
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
void DrawScreen() {
    uint16_t *vga_text = (uint16_t *)0xB8000;
    // clear screen
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x1f00;
    // draw border
    for (int c = 1; c < 79; c++) {
        vga_text[c] = 0x1fc4; // top line
        vga_text[(2*80)+c] = 0x1fc4; // 3rd line
        vga_text[(24*80)+c] = 0x1fc4; // bottom line
    }
    for (int l = 1; l < 24; l++) {
        vga_text[80*l] = 0x1fb3;
        vga_text[(80*l)+79] = 0x1fb3;
    }
    vga_text[0] = 0x1fda;
    vga_text[79] = 0x1fbf;
    vga_text[2*80] = 0x1fc3;
    vga_text[2*80+79] = 0x1fb4;
    vga_text[24*80] = 0x1fc0;
    vga_text[24*80+79] = 0x1fd9;
    // name
    vga_text[80+34] = 0x1f00 | '-';
    vga_text[80+35] = 0x1f00 | ' ';
    vga_text[80+36] = 0x1f00 | 'L';
    vga_text[80+37] = 0x1f00 | 'u';
    vga_text[80+38] = 0x1f00 | 'c';
    vga_text[80+39] = 0x1f00 | 'i';
    vga_text[80+40] = 0x1f00 | 'a';
    vga_text[80+41] = 0x1f00 | 'O';
    vga_text[80+42] = 0x1f00 | 'S';
    vga_text[80+43] = 0x1f00 | ' ';
    vga_text[80+44] = 0x1f00 | '-';
}

int32_t fileCount;
DIRENT *entries = (DIRENT*)0x400000;
void PrintFileList() {
    uint16_t *vga_text = &((uint16_t *)0xb8000)[80*6+3];
    for (int i = 0; i < fileCount; i++) {
        DIRENT *de = &entries[i];
        for (int i = 0; i < 11 && de->name[i]; i++) {
            if (i == 8) { *(uint8_t*)vga_text = ' '; vga_text++; } // space for 8.3
            *(uint8_t *)vga_text = de->name[i];
            vga_text++;
        }
        vga_text += printStr("  ", vga_text);
        vga_text += printDec((uint32_t)de->filesize_0 +
                ((uint32_t)de->filesize_1 << 8) +
                ((uint32_t)de->filesize_2 << 16) +
                ((uint32_t)de->filesize_3 << 24), vga_text);
        *(uint8_t*)vga_text++ = 'B';
        vga_text = nextLine(vga_text) + 3;
    }
}
extern void create_child(uint32_t esp, uint32_t eip);
void FileSelect() {
    fileCount = 5;
    uint16_t *vga_text = (uint16_t *)0xb8000;
    int32_t fileHovered = 0, lastFileHovered = 0;
    for (char reload = 1;;) {
        // Info line (4)
        printStr("T to run tests - X to view in hex - V to view as text", &vga_text[80*4+2]);
        VOLINFO vi; DIRINFO di;
        if (reload) {
            OpenVol(&vi);
            OpenDir((uint8_t*)"", &vi, &di);
            GetFileList(entries, &fileCount, &vi, &di);
            reload = 0;
        }
        PrintFileList();
        if (lastFileHovered != fileHovered) {
            *(uint8_t*)&vga_text[80*(6+lastFileHovered)+2] = ' ';
            lastFileHovered = fileHovered;
        }
        *(uint8_t*)&vga_text[80*(6+fileHovered)+2] = '>';
        uint16_t key = get_scancode();
        uint8_t path[13];
        switch (key & 0xff) { // scancode component
            case 0x50: // down
                fileHovered++;
                if (fileHovered >= fileCount) fileHovered = 0;
                break;
            case 0x48: // up
                fileHovered--;
                if (fileHovered < 0) fileHovered = fileCount - 1;
                break;
            case 0x14: // t
                create_child(0x3C0000, (uintptr_t)RunTests);
                SetCursorDisabled();
                DrawScreen();
                reload = 1;
                break;
            case KEY_X:
                File83ToPath((char*)entries[fileHovered].name, (char*)path);
                HexViewTest(path, &vi);
                SetVideo25Lines();
                SetCursorDisabled();
                DrawScreen();
                break;
            case KEY_V:
                File83ToPath((char*)entries[fileHovered].name, (char*)path);
                TextViewTest(path, &vi);
                SetVideo25Lines();
                SetCursorDisabled();
                DrawScreen();
                reload = 1;
                break;
            default:
                break;
        }
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
    backup_ivtbios();

    vga_text = &((word *)0xb8000)[80];
    vga_text += printStr("Press T for tests, or any key to continue... ", vga_text);
    uint8_t key = get_key();
    if (key == 't' || key == 'T')
        RunTests(vga_text);
    DrawScreen();
    SetVideo25Lines();
    SetCursorDisabled();
    FileSelect();
}

