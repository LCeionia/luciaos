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
    uint16_t *vga_text = (uint16_t *)0xb8000 + (80*2);
    vga_text += printStr("Done.", vga_text);
}
extern char _USERMODE;
extern char _binary_usermode_bin_start, _binary_usermode_bin_end;
void ReloadUser() {
    // Put Usermode code in proper place based on linker
    char *s = &_binary_usermode_bin_start;
    char *d = (char *)&_USERMODE;
    while (s < &_binary_usermode_bin_end)
        *d++ = *s++;
}
char TestUser() {
    ReloadUser();
    char *vga = (char *)(uintptr_t)create_user_child(0x800000, (uintptr_t)&_USERMODE, 0);
    if ((uintptr_t)vga != 0xA0000) {
        return 1;
    }
    for (int i = 0; i < 320; i++) {
        vga[i] = i;
    }
    return 0;
}
void TestDiskRead() {
    char *diskReadBuf = (char *)0x20000;
    v86disk_addr_packet.transfer_buffer =
        (uintptr_t)diskReadBuf & 0x000F |
        (((uintptr_t)diskReadBuf & 0xFFFF0) << 12);
    v86disk_addr_packet.start_block = 0;
    v86disk_addr_packet.blocks = 2;
    union V86Regs_t regs;
    kbd_clear();
    for (;;) {
        regs.h.ah = 0x42;
        FARPTR v86_entry = i386LinearToFp(v86DiskOp);
        enter_v86(0x0000, 0x8000, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
        uint16_t *vga_text = (uint16_t *)0xb8000;
        for (int i = 0; i < 25; i++) {
            uint16_t *vga_line = &vga_text[i*80];
            for (int j = 0; j < 26; j++) {
                printByte(diskReadBuf[i*26 + j], &vga_line[j*2]);
                *(uint8_t*)&vga_line[j + (26*2)+1] = diskReadBuf[i*26 + j];
            }
        }
        uint16_t scan = get_scancode();
        if ((scan & 0xff) == KEY_PGUP) {
            if(v86disk_addr_packet.start_block > 0) v86disk_addr_packet.start_block--;
        } else if ((scan & 0xff) == KEY_PGDOWN)
            v86disk_addr_packet.start_block++;
        else if (scan & 0xff00) break;
    }
}
extern uint32_t _gpf_eax_save;
extern uint32_t _gpf_eflags_save;
void TestCHS() {
    uint16_t *vga_text = (uint16_t*)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    printStr("CHS Test ", vga_text);
    // CHS Read
    union V86Regs_t regs;
    FARPTR v86_entry = i386LinearToFp(v86DiskGetGeometry);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
    // check CF for error
    if (_gpf_eflags_save & 0x1) {
        uint16_t *vga = &((uint16_t*)0xb8000)[80];
        vga += printStr("Could not get Disk Geometry.", vga);
        return;
    }
    uint32_t numHead = ((_gpf_eax_save & 0xff00) >> 8) + 1;
    uint32_t secPerTrack = _gpf_eax_save & 0x3f;
    uint32_t maxCyl = ((_gpf_eax_save & 0xff0000) >> 16) | ((_gpf_eax_save & 0xc0) << 2);
    uint32_t maxLBA = numHead * secPerTrack * (maxCyl + 1);
    for (uint32_t sector = 0; sector < maxLBA && sector < 0x8000 /* don't bother reading huge disks */; sector += (sector + secPerTrack < maxLBA) ? secPerTrack : 1) {
        uint16_t *vga = &((uint16_t*)0xb8000)[9];
        vga += printWord(sector, vga); vga++;
        // LBA -> CHS
        uint32_t tmp = sector / secPerTrack;
        uint32_t sec = (sector % (secPerTrack)) + 1;
        uint32_t head = tmp % numHead;
        uint32_t cyl = tmp / numHead;
        vga += printWord(cyl, vga); vga += printChar(':', vga);
        vga += printByte(head, vga); vga += printChar(':', vga);
        vga += printByte(sec, vga); vga += printChar(' ', vga);
        vga += printStr("/ ", vga);
        vga += printWord(maxCyl, vga); vga += printChar(':', vga);
        vga += printByte(numHead - 1, vga); vga += printChar(':', vga);
        vga += printByte(secPerTrack, vga); vga += printChar(' ', vga);
        regs.w.ax = 0x0201;
        regs.h.ch = cyl & 0xff;
        regs.h.cl = sec | ((cyl >> 2) & 0xc0);
        regs.h.dh = head;
        regs.h.dl = 0x80;
        regs.w.bx = 0x0000;
        v86_entry = i386LinearToFp(v86DiskReadCHS);
        enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
        // if CF was set, we have error
        if (_gpf_eflags_save & 0x1) {
            uint16_t *vga = &((uint16_t*)0xb8000)[80];
            vga += printStr("Disk Error: ", vga);
            vga += printByte(_gpf_eax_save & 0xff, vga);
            vga += printStr(" CHS=", vga);
            vga += printWord(cyl, vga); vga += printChar(':', vga);
            vga += printByte(head, vga); vga += printChar(':', vga);
            vga += printByte(sec, vga);
            return;
        }
    }
}

//void TestFAT() {
//    uint16_t *vga_text = (uint16_t *)0xb8000;
//    uint8_t *diskReadBuf = (uint8_t *)0x22400;
//    for (int i = 0; i < 80*25; i++)
//        vga_text[i] = 0x0f00;
//    VOLINFO vi;
//
//    uint8_t pactive, ptype;
//    uint32_t pstart, psize;
//    pstart = DFS_GetPtnStart(0, diskReadBuf, SystemPartition, &pactive, &ptype, &psize);
//    vga_text = (uint16_t *)0xb8000;
//    vga_text += printStr("PartStart: ", vga_text);
//    vga_text += printDword(pstart, vga_text);
//    vga_text += 2;
//    vga_text += printStr("PartSize: ", vga_text);
//    vga_text += printDword(psize, vga_text);
//    vga_text += 2;
//    vga_text += printStr("PartActive: ", vga_text);
//    vga_text += printByte(pactive, vga_text);
//    vga_text += 2;
//    vga_text += printStr("PartType: ", vga_text);
//    vga_text += printByte(ptype, vga_text);
//    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
//    //asm ("xchgw %bx, %bx");
//
//    DFS_GetVolInfo(0, diskReadBuf, pstart, &vi);
//    vga_text += printStr("Label: ", vga_text);
//    vga_text += printStr((char*)vi.label, vga_text);
//    vga_text += 2;
//    vga_text += printStr("Sec/Clus: ", vga_text);
//    vga_text += printByte(vi.secperclus, vga_text);
//    vga_text += 2;
//    vga_text += printStr("ResrvSec: ", vga_text);
//    vga_text += printWord(vi.reservedsecs, vga_text);
//    vga_text += 2;
//    vga_text += printStr("NumSec: ", vga_text);
//    vga_text += printDword(vi.numsecs, vga_text);
//    vga_text += 2;
//    vga_text += printStr("Sec/FAT: ", vga_text);
//    vga_text += printDword(vi.secperfat, vga_text);
//    vga_text += 2;
//    vga_text += printStr("FAT1@: ", vga_text);
//    vga_text += printDword(vi.fat1, vga_text);
//    vga_text += 2;
//    vga_text += printStr("ROOT@: ", vga_text);
//    vga_text += printDword(vi.rootdir, vga_text);
//    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
//    //asm ("xchgw %bx, %bx");
//
//    vga_text += printStr("Files in root:", vga_text);
//    DIRINFO di;
//    di.scratch = diskReadBuf;
//    DFS_OpenDir(&vi, (uint8_t*)"", &di);
//    vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
//    DIRENT de;
//    while (!DFS_GetNext(&vi, &di, &de)) {
//        if (de.name[0]) {
//            for (int i = 0; i < 11 && de.name[i]; i++) {
//                if (i == 8) { *(uint8_t*)vga_text = ' '; vga_text++; } // space for 8.3
//                *(uint8_t *)vga_text = de.name[i];
//                vga_text++;
//            }
//            vga_text += printStr("  ", vga_text);
//            vga_text += printDec((uint32_t)de.filesize_0 + ((uint32_t)de.filesize_1 << 8) + ((uint32_t)de.filesize_2 << 16) + ((uint32_t)de.filesize_3 << 24), vga_text);
//            *(uint8_t*)vga_text++ = 'B';
//            vga_text = (uint16_t *)((((((uintptr_t)vga_text)-0xb8000) - ((((uintptr_t)vga_text)-0xb8000) % 160)) + 160)+0xb8000);
//        }
//        //asm ("xchgw %bx, %bx");
//    }
//}

void RunTests() {
    char doTests = 1;
    uint16_t *vga_text = (uint16_t*)0xb8000;
    uint8_t key;
    for (char l = 1;l;) {
        if (doTests) {
            vga_text = (uint16_t*)0xb8000;
            for (int i = 0; i < 80*25; i++)
                vga_text[i] = 0x1f00;
            vga_text += printStr("V86 Test... ", vga_text);
            //asm ("xchgw %bx, %bx");
            TestV86(); // has int 3 wait in v86
            vga_text = (uint16_t *)0xb8000 + (80*3);
            vga_text += printStr("Press 'N' for next test.", vga_text);
            for(;;) {
                key = get_key();
                if (key == 'N' || key == 'n') break;
                *vga_text = (*vga_text & 0xFF00) | key;
                vga_text++;
            }
            if (TestUser()) {
                union V86Regs_t regs;
                regs.w.ax = 3; // text mode
                V8086Int(0x10, &regs); 
                vga_text = (uint16_t *)0xb8000 + (80*5);
                printStr("Usermode test failed! Press any key to continue.", vga_text);
                kbd_wait();
            } else {
                kbd_wait();
                union V86Regs_t regs;
                regs.w.ax = 3; // text mode
                V8086Int(0x10, &regs); 
            }
            doTests = 0;
        }

        vga_text = &((uint16_t*)0xB8000)[80*14];
        vga_text += printStr("Press E for a flagrant system error.  ", vga_text);
        vga_text = &((uint16_t*)0xB8000)[80*15];
        vga_text += printStr("Press D for disk tests.               ", vga_text);
        vga_text = &((uint16_t*)0xB8000)[80*16];
        vga_text += printStr("Press R to repeat tests.              ", vga_text);
        vga_text = &((uint16_t*)0xB8000)[80*17];
        vga_text += printStr("Press C to continue...                ", vga_text);
        switch (key = get_key()) {
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
            case 'd':
            case 'D':
                // disk tests
                TestCHS();
                kbd_wait();
                TestDiskRead();
                //TestFAT();
                break;
            case 'r':
            case 'R':
                doTests = 1;
                break;
            default:
                *vga_text = (*vga_text & 0xFF00) | key;
                vga_text++;
                break;
        }
    }
}

