#include "progs.h"
#include "file.h"

extern char _USERMODE, _USERMODE_END;
extern uint32_t create_user_child(uint32_t esp, uint32_t eip, uint32_t argc, ...);
void ProgramLoadTest(char *path) {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    uint32_t successcount;
    uint8_t *diskReadBuf = (uint8_t *)&_USERMODE;
    {
        uint32_t err;
        dirent de;
        err = path_getinfo(path, &de);
        if (de.size > 0x300000 || err) {
            vga_text += printStr("File too large or error.", vga_text);
            kbd_wait();
            return;
        }
        FILE file;
        err = file_open(&file, path, OPENREAD);
        if (err) {
            vga_text += printStr("Open Error: ", vga_text);
            printDword(err, vga_text);
            return;
        }
        err = file_seek(&file, 0);
        if (err) {
            vga_text += printStr("Seek Error", vga_text);
            return;
        }
        successcount = err = file_read(&file, diskReadBuf, de.size);
        if (!err && de.size > 0) {
            vga_text += printStr("Read Error", vga_text);
            printDword(err, vga_text);
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
    uint32_t res = create_user_child((uintptr_t)&_USERMODE_END, (uintptr_t)&_USERMODE, 0);
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
