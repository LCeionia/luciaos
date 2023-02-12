#include "progs.h"

// 400000 - 700000 Usermode Code (3mB)
// 700000 - 800000 Usermode Stack (1mB)
extern char _USERMODE;
extern uint32_t create_user_child(uint32_t esp, uint32_t eip, uint32_t argc, ...);
void ProgramLoadTest(uint8_t *path, VOLINFO *vi) {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    uint32_t successcount;
    uint8_t *diskReadBuf = (uint8_t *)&_USERMODE;
    {
        uint32_t err;
        uint8_t *scratch = (uint8_t *)0x20000;
        FILEINFO fi;
        err = DFS_OpenFile(vi, path, DFS_READ, scratch, &fi);
        if (err) {
            vga_text += printStr("Open Error: ", vga_text);
            printDword(err, vga_text);
            return;
        }
        if (fi.filelen > 0x300000) {
            vga_text += printStr("File too large.", vga_text);
            kbd_wait();
            return;
        }
        DFS_Seek(&fi, 0, scratch);
        if (fi.pointer != 0) {
            vga_text += printStr("Seek Error", vga_text);
            return;
        }
        err = DFS_ReadFile(&fi, scratch, diskReadBuf, &successcount, fi.filelen);
        if (err && err != DFS_EOF) {
            vga_text += printStr("Read Error: ", vga_text);
            printDword(err, vga_text);
            return;
        }
        if (successcount < fi.filelen) {
            vga_text += printStr("Could not read all file bytes.", vga_text);
            return;
        }
    }
    vga_text += printStr("Successfully loaded program \"", vga_text);
    vga_text += printStr((char*)path, vga_text);
    vga_text += printStr("\", ", vga_text);
    vga_text += printDec(successcount, vga_text);
    vga_text += printStr(" Bytes.", vga_text);
    vga_text = nextLine(vga_text,(uint16_t*)0xb8000);
    vga_text += printStr("Press any key to run.", vga_text);
    kbd_wait();
    uint32_t res = create_user_child(0x800000, (uintptr_t)&_USERMODE, 0);
    union V86Regs_t regs;
    regs.w.ax = 3; // text mode
    V8086Int(0x10, &regs); 
    vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    vga_text += printStr("Program returned code: ", vga_text);
    vga_text += printDec(res, vga_text);
    vga_text += printStr(" (0x", vga_text);
    vga_text += printDword(res, vga_text);
    vga_text += printStr("). Press any key to exit.", vga_text);
    kbd_wait();
}
