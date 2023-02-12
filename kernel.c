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
#include "disk.h"

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
char check_cmov() {
    uint32_t eax, ebx, ecx, edx;
    asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx & (1 << 15)) != 0;
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

extern char _edata, _v86code, _ev86code, _bstart, _bend;
void setup_binary() {
    // Put V86 code in proper place based on linker
    char *s = &_edata;
    char *d = &_v86code;
    while (d < &_ev86code)
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
}

__attribute__((__no_caller_saved_registers__))
__attribute__((__noreturn__))
extern void return_prev_task();
__attribute__((__no_caller_saved_registers__))
__attribute__((__noreturn__))
void error_environment(uint32_t stack0, uint32_t stack1, uint32_t stack2, uint32_t stack3, uint32_t stack4, uint32_t stack5) {
    ensure_v86env();
    setup_interrupts(); // just in case
    for (int i = 0; i < 80*50; i++)
        if (!(error_screen[i] & 0xFF00))
            error_screen[i] = 0x0f00 | (error_screen[i] & 0x00FF);
    union V86Regs_t regs;
    FARPTR v86_entry = i386LinearToFp(v86TextMode);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
    char str0[] = "Oh noes!!! System error! ;c     STKDMP:";
    printStr(str0, &error_screen[80]);
    uint16_t *tmp = &error_screen[80+sizeof(str0)-1];
    tmp += printDword(stack0, tmp);
    tmp++;
    tmp += printDword(stack1, tmp);
    tmp++;
    tmp += printDword(stack2, tmp);
    tmp = &error_screen[80*2+sizeof(str0)-1];
    tmp += printDword(stack3, tmp);
    tmp++;
    tmp += printDword(stack4, tmp);
    tmp++;
    tmp += printDword(stack5, tmp);
    printStr("Press E for a fun recovery :3", &error_screen[80*2]);
    printStr("Press R to return to previous task", &error_screen[80*3]);
    uint16_t *vga_text = ((uint16_t*)0xB8000);
    for (int i = 0; i < 80*50; i++)
        vga_text[i] = error_screen[i];
    for(;;) {
        uint8_t key = get_scancode() & 0xff;
        if (key == KEY_E) {
            v86_entry = i386LinearToFp(v86TransFlag);
            enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
        }
        if (key == KEY_R) break;
    }
    // reset error screen
    for (int i = 0; i < (80*50)/2; i++)
        ((uint32_t*)error_screen)[i] = 0x0f000f00;
    return_prev_task();
}

uint32_t GetFreeStack() {
    uint32_t stack;
    asm volatile("mov %%esp,%%eax":"=a"(stack));
    stack = ((stack - 0x2000) / 0x1000) * 0x1000;
    return stack;
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
202080 - 300000 Free (~1/2mB)
280000 - 300000 Disk Cache (512kB)
300000 - 310000 Task Stack (64kB)
310000 - 320000 Interrupt Stack (64kB)
320000 - 400000 Kernel Stack (896kB)
400000 - 700000 Usermode Code (3mB)
700000 - 800000 Usermode Stack (1mB)
*/
void DrawScreen(uint16_t *vga) {
    uint16_t *vga_text = vga;
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

void SetPalette() {
    union V86Regs_t regs;
    regs.w.bx = 1; // index
    regs.h.dh = 0x3D;
    regs.h.ch = 0x2A;
    regs.h.cl = 0x2E;
    regs.w.ax = 0x1010; // set DAC color register
    V8086Int(0x10, &regs);
}
// NOTE Does not set text mode
void RestoreVGA() {
    SetVideo25Lines();
    SetCursorDisabled();
    SetPalette();
}

int32_t fileCount, fileOffset;
// We store dir entries in usermode space,
// which is nice because there's nothing after,
// but it does mean we need to reload the dir
// after every task called. This might be fine,
// since the task might have modified the directory.
extern char _USERMODE;
DIRENT *const DirEntries = (DIRENT*)&_USERMODE;
#define MAXDISPFILES 16
void PrintFileList(uint16_t *vga) {
    uint16_t *vga_text = &((uint16_t *)vga)[80*6+3];
    for (int i = 0; (i + fileOffset) < fileCount && i < MAXDISPFILES; i++) {
        DIRENT *de = &DirEntries[i + fileOffset];
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
        vga_text = nextLine(vga_text, vga) + 3;
    }
}
char IsDir(DIRENT *de) {
    return de->attr & ATTR_DIRECTORY;
}
void ScancodeTest() {
    uint16_t *vga = (uint16_t*)0xb8000;
    uint16_t scancode;
    while(((scancode = get_scancode()) & 0xff) != KEY_F1) {
        vga += printWord(scancode, vga);
        vga++;
        if ((uintptr_t)vga >= 0xb8000+80*25*2)
            vga = (uint16_t*)0xb8000;
    }
}
extern void create_child(uint32_t esp, uint32_t eip, uint32_t argc, ...);
uint16_t FileSelectScreen[80*25];
void FileSelect() {
    uint8_t current_path[80];
    uintptr_t current_path_end;
    for (int i = 0; i < sizeof(current_path); i++)
        current_path[i] = 0;
    current_path[0] = '/';
    current_path_end = 1;
    fileCount = 5;
    uint16_t *vga_text = (uint16_t *)FileSelectScreen;
    int32_t fileHovered = 0;
    fileOffset = 0;
    for (char reload = 1;;) {
        DrawScreen(vga_text);
        // Info line (4)
        {
            uint16_t *vga = &vga_text[80*4 + 79 - 24];
            printStr("X to view in hex", vga);
            vga += 80;
            printStr("T to view as text", vga);
            vga += 80;
            printStr("P to load as program", vga);
            vga += 80;
            printStr("O to open directory", vga);
            vga += 80;
            printStr("F4 to run tests", vga);
        }
        printStr((char*)current_path, &vga_text[80*4 + 2]);
        for (int i = 2; i < 15; i++)
            *(uint8_t*)&vga_text[80*5 + i] = '-';
        VOLINFO vi; DIRINFO di;
        if (reload) {
            OpenVol(&vi);
            current_path[current_path_end] = 0;
            OpenDir(current_path, &vi, &di);
            GetFileList(DirEntries, &fileCount, INT32_MAX, &vi, &di);
            reload = 0;
        }
        if (fileHovered >= fileCount) {
            fileOffset = fileCount - MAXDISPFILES;
            fileHovered = fileCount - 1;
        }
        if ((fileHovered - fileOffset) >= MAXDISPFILES)
            fileOffset = fileHovered - MAXDISPFILES + 1;
        else if ((fileHovered - fileOffset) < 0)
            fileOffset = fileHovered;
        PrintFileList(vga_text);
        for (int i = 6; i < 24; i++) {
            *(uint8_t*)&vga_text[80*i+2] = ' ';
        }
        *(uint8_t*)&vga_text[80*(6+(fileHovered-fileOffset))+2] = '>';
        // Copy to real VGA
        for (int i = 0; i < 80*25; i++)
            ((uint16_t*)0xb8000)[i] = vga_text[i];
        uint16_t key = get_scancode();
        uint8_t path[13];
        switch (key & 0xff) { // scancode component
            case KEY_DOWN: // down
                fileHovered++;
                if (fileHovered >= fileCount) { fileHovered = 0; fileOffset = 0; }
                break;
            case KEY_UP: // up
                fileHovered--;
                if (fileHovered < 0) fileHovered = fileCount - 1;
                break;
            case KEY_F4:
                create_child(GetFreeStack(), (uintptr_t)RunTests, 0);
                // Set Text mode, tests might return in gfx
                {
                    union V86Regs_t regs;
                    regs.w.ax = 3;
                    V8086Int(0x10, &regs);
                }
                RestoreVGA();
                reload = 1;
                break;
            case KEY_P:
                if (IsDir(&DirEntries[fileHovered])) break;
                File83ToPath((char*)DirEntries[fileHovered].name, (char*)&current_path[current_path_end]);
                create_child(GetFreeStack(), (uintptr_t)ProgramLoadTest, 2, current_path, &vi);
                RestoreVGA();
                reload = 1;
                break;
            case KEY_X:
                if (IsDir(&DirEntries[fileHovered])) break;
                File83ToPath((char*)DirEntries[fileHovered].name, (char*)&current_path[current_path_end]);
                create_child(GetFreeStack(), (uintptr_t)HexEditor, 2, current_path, &vi);
                RestoreVGA();
                reload = 1;
                break;
            case KEY_T:
                if (IsDir(&DirEntries[fileHovered])) break;
                File83ToPath((char*)DirEntries[fileHovered].name, (char*)&current_path[current_path_end]);
                //TextViewTest(path, &vi);
                create_child(GetFreeStack(), (uintptr_t)TextViewTest, 2, current_path, &vi);
                RestoreVGA();
                reload = 1;
                break;
            case KEY_O:
            case 0x9C: // enter release
                if (IsDir(&DirEntries[fileHovered])) {
                    uint8_t tmp_path[80];
                    File83ToPath((char*)DirEntries[fileHovered].name, (char*)tmp_path);
                    if ((*(uint32_t*)tmp_path & 0xffff) == ('.' | 0x0000)) {
                        // Current dir, do nothing
                        break;
                    } else if ((*(uint32_t*)tmp_path & 0xffffff) == ('.' | '.' << 8 | 0x000000)) {
                        // Up one directory
                        for (current_path_end--;current_path_end >= 1 && current_path[current_path_end - 1] != '/'; current_path_end--);
                        current_path[current_path_end] = 0;
                        reload = 1;
                        fileHovered = 0;
                        fileOffset = 0;
                        break;
                    }
                    for (int i = 0; (i + current_path_end) < sizeof(current_path); i++)
                        current_path[current_path_end + i] = tmp_path[i];
                    for (;current_path_end < sizeof(current_path) - 2 && current_path[current_path_end]; current_path_end++);
                    current_path[current_path_end] = '/';
                    current_path_end++;
                    current_path[current_path_end] = 0;
                    reload = 1;
                    fileHovered = 0;
                    fileOffset = 0;
                }
                break;
            case KEY_F6:
                ScancodeTest();
                reload = 1;
                break;
            default:
                break;
        }
    }
}

void SystemRun() {
    uint16_t *vga_text = (word *)0xb8000;
    RestoreVGA();
    DrawScreen((uint16_t*)0xb8000);

    // Check for FAT partition
    {
        VOLINFO vi;
        // TODO Check partitions beyond 0
        while (OpenVol(&vi)) {
            vga_text = &((word*)0xb8000)[80*4 + 2];
            vga_text += printStr("Error loading file select. Ensure the disk has a valid MBR and FAT partition.", vga_text);
            vga_text = &((word*)0xb8000)[80*5 + 2];
            vga_text += printStr("Press R to retry.", vga_text);
            for (;(get_scancode() & 0xff) != KEY_R;);
        }
    }

    for (;;) {
        create_child(GetFreeStack(), (uintptr_t)FileSelect, 0);
        // should never return, so if it does,
        // we have an error
        {
            union V86Regs_t regs;
            FARPTR v86_entry = i386LinearToFp(v86TextMode);
            enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
        }
        RestoreVGA();
        DrawScreen((uint16_t*)0xb8000);
        vga_text = &((word*)0xb8000)[80*4 + 2];
        vga_text += printStr("Error loading file select. Ensure the disk has a valid MBR and FAT partition.", vga_text);
        vga_text = &((word*)0xb8000)[80*5 + 2];
        vga_text += printStr("Press R to retry.", vga_text);
        for (;(get_scancode() & 0xff) != KEY_R;);
    }
}

__attribute__((__noreturn__))
extern void triple_fault();
uint32_t kernel_check = 0x12345678;
void start() {
    word *vga_text = (word *)0xb8000;
    char h[] = "LuciaOS";
    for (int i = 0; i < sizeof(h); i++)
        *(char *)&vga_text[i] = h[i];
    vga_text = &vga_text[80];

    // DL *should* be preserved
    uint8_t dl;
    asm volatile("nop":"=d"(dl));

    if (!check_cmov()) {
        char cmov_err[] = "NO CMOV";
        for (int i = 0; i < sizeof(cmov_err); i++)
            *(char *)&vga_text[i] = cmov_err[i];
        for (;;) asm volatile("hlt");
    }

    vga_text += printByte(dl, vga_text);
    vga_text++;

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

    //char sse_str[] = "SSE support: ";
    //vga_text += printStr("SSE: ", vga_text);
    //char sse = check_sse();
    //if (!sse) {
    //    *vga_text = 'N';
    //}
    //vga_text += printStr("Y ", vga_text);
    //enable_sse();

    //print_flags();
    vga_text += printStr("CR0:", vga_text);
    vga_text += printDword(get_cr0(), vga_text);
    vga_text++;
    //print_cr3();
    //print_cr4();

    // Setup system
    setup_binary();
    setup_interrupts();
    setup_tss();
    init_paging();
    backup_ivtbios();
    InitDisk();

    create_child(GetFreeStack(), (uintptr_t)SystemRun, 0);
    // If this returns, something is *very* wrong, reboot the system
    // TODO Maybe try to recover?

    // Triple fault
    triple_fault();
}

