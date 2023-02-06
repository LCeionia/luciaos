#include "tests.h"

extern char *user_test();
extern uint32_t create_user_child(uint32_t esp, uint32_t eip, uint32_t argc, ...);

void TestV86() {
    union V86Regs_t regs;
    regs.d.edi = 0x11111111;
    regs.d.esi = 0x22222222;
    regs.d.ebx = 0x33333333;
    regs.d.edx = 0x44444444;
    regs.d.ecx = 0x55555555;
    regs.d.eax = 0x66666666;
    FARPTR v86_entry = i386LinearToFp(v86Test);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
}
extern char _loadusercode, _usercode, _eusercode;
void ReloadUser() {
    // Put Usermode code in proper place based on linker
    char *s = &_loadusercode;
    char *d = &_usercode;
    while (d < &_eusercode)
        *d++ = *s++;
}
char TestUser() {
    ReloadUser();
    char *vga = (char *)(uintptr_t)create_user_child(0x800000, (uintptr_t)user_test, 0);
    if ((uintptr_t)vga != 0xA0000) {
        return 1;
    }
    for (int i = 0; i < 320; i++) {
        vga[i] = i;
    }
    return 0;
}
void TestDiskRead() {
    union V86Regs_t regs;
    uint16_t *vga_text = (uint16_t *)0xb8000 + (80*5);
    vga_text += printStr("Setting Text Mode... ", vga_text);
    regs.w.ax = 3; // text mode
    V8086Int(0x10, &regs); 
    vga_text += printStr("Done. Starting Disk Read... ", vga_text);
    char *diskReadBuf = (char *)0x23000;
	v86disk_addr_packet.transfer_buffer =
		(uintptr_t)diskReadBuf & 0x000F |
		(((uintptr_t)diskReadBuf & 0xFFFF0) << 12);
    FARPTR v86_entry = i386LinearToFp(v86DiskRead);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
    vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < (80*25)/2; i++) {
        printByte(diskReadBuf[i], &vga_text[i*2]);
    }
}
void TestFAT() {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    uint8_t *diskReadBuf = (uint8_t *)0x22400;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    VOLINFO vi;

    uint8_t pactive, ptype;
    uint32_t pstart, psize;
    pstart = DFS_GetPtnStart(0, diskReadBuf, 0, &pactive, &ptype, &psize);
    vga_text = (uint16_t *)0xb8000;
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
    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
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
    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
    //asm ("xchgw %bx, %bx");

    vga_text += printStr("Files in root:", vga_text);
    DIRINFO di;
    di.scratch = diskReadBuf;
    DFS_OpenDir(&vi, (uint8_t*)"", &di);
    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
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
            vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
        }
        //asm ("xchgw %bx, %bx");
    }
}

void RunTests() {
    uint16_t *vga_text = (uint16_t*)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x1f00;
    uint8_t key;
    vga_text += printStr("V86 Test... ", vga_text);
    //asm ("xchgw %bx, %bx");
    TestV86(); // has int 3 wait in v86
    vga_text = (uint16_t *)0xb8000 + (80*3);
    vga_text += printStr("Done. Press 'N' for next test.", vga_text);
    for(;;) {
        key = get_key();
        if (key == 'N' || key == 'n') break;
        *vga_text = (*vga_text & 0xFF00) | key;
        vga_text++;
    }
    if (TestUser()) {
        // Usermode returned wrong value
        vga_text = nextLine(vga_text);
        vga_text += printStr("Usermode test failed! Press any key to continue.", vga_text);
    }
    kbd_wait();
    TestDiskRead();
    kbd_wait();
    TestFAT();

    vga_text = &((uint16_t*)0xB8000)[80*16];
    vga_text += printStr("Press E for a flagrant system error. Press C to continue... ", vga_text);
    for (char l = 1;l;) { switch (key = get_key()) {
        case 'e':
        case 'E':
            // flagrant system error
            *((uint8_t*)0x1000000) = 0;
            break;
        case 'c':
        case 'C':
            // continue
            l = 0;
            break;
        default:
            *vga_text = (*vga_text & 0xFF00) | key;
            vga_text++;
            break;
    }}
}

