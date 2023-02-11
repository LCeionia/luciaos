#include "progs.h"

void TextViewTest(uint8_t *path, VOLINFO *vi) {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    uint32_t fileLen;
    uint8_t *diskReadBuf = (uint8_t *)0x500000;
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
        // file too large
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
        err = DFS_ReadFile(&fi, scratch, diskReadBuf, &fileLen, fi.filelen);
        if (err && err != DFS_EOF) {
            vga_text += printStr("Read Error: ", vga_text);
            printDword(err, vga_text);
            return;
        }
    }
    uint32_t *lineOffsets = (uint32_t *)0x400000;
    uint32_t lastLine;
    {
        char nl;
        uint8_t c = 0x0A; // start with a pretend newline
        uint32_t line = -1; // start a pretend line behind
        for (int32_t o = -1; o < (int32_t)fileLen; c = diskReadBuf[++o]) {
            // newline
            if (c == 0x0A) {
                lineOffsets[++line] = o;
            }
            // file too large
            if ((uintptr_t)&lineOffsets[line] >= 0x4FFFFC) {
                vga_text += printStr("File too large.", vga_text);
                kbd_wait();
                return;
            }
        }
        lastLine = line;
    }
    uint32_t currLine = 0;
    char cont = 1;
    uint32_t screenSize = 80*25;
    char redraw = 1;
    uint32_t linesOnScreen = 0;
    for (;cont;) {
        if (redraw) {
            vga_text = (uint16_t *)0xb8000;
            for (int i = 0; i < screenSize; i++)
                vga_text[i] = 0x0f00;
            char pathBuff[22];
            trimPath((char*)path, pathBuff, sizeof(pathBuff));
            vga_text += printStr(pathBuff, vga_text);
            vga_text += 2;
            vga_text += printDec(currLine, vga_text);
            vga_text += printChar('/', vga_text);
            vga_text += printDec(lastLine, vga_text);
            vga_text += printStr(" Scroll: Up/Down PgUp/PgDown Home/End", vga_text);
            {
                const char prnt[] = "Exit: E";
                vga_text = &((uint16_t*)0xb8000)[80-sizeof(prnt)+1];
                vga_text += printStr((char*)prnt, vga_text);
            }
            for (vga_text = &((uint16_t*)0xb8000)[84]; vga_text < &((uint16_t*)0xb8000)[screenSize]; vga_text += 80)
                *(uint8_t*)vga_text = '|';
            vga_text = &((uint16_t*)0xb8000)[0];
            uint32_t lineOff = 6;
            uint8_t c = 0x0A; // start with a pretend newline
            uint32_t line = currLine - 1; // start a pretend line behind
            int32_t o = lineOffsets[currLine]; // the real or fake newline on previous line
            linesOnScreen = screenSize/80;
            for (; o < (int32_t)fileLen && vga_text < &((uint16_t*)0xb8000)[screenSize]; c = diskReadBuf[++o]) {
                // newline
                if (c == 0x0A) {
                    vga_text = nextLine(vga_text,(uint16_t*)0xb8000);
                    line++;
                    {
                        uint16_t *vga_tmp = vga_text;
                        uint16_t decTmp[11];
                        char cnt = printDec(line, decTmp);
                        char off = cnt <= 4 ? 0 : cnt - 4;
                        vga_tmp += 4 - (cnt - off);
                        for (int i = off; i < cnt; i++, vga_tmp++)
                            *(uint8_t*)vga_tmp = (uint8_t)decTmp[i];
                    }
                    vga_text += 6;
                    lineOff = 6;
                    continue;
                } 
                *(uint8_t*)vga_text = c;
                vga_text++;
                lineOff++;
                if (lineOff == 80) { // last char
                    vga_text += 6;
                    lineOff = 6;
                    linesOnScreen--;
                }
            }
            redraw = 0;
        }
        uint16_t key = get_scancode();
        union V86Regs_t regs;
        FARPTR v86_entry;
        switch (key & 0xff) {
            case KEY_DOWN: // down
                if (currLine < lastLine && lineOffsets[currLine+1] < fileLen) {
                    currLine++;
                    redraw = 1;
                }
                break;
            case KEY_UP: // up
                if ((currLine > 0 && lineOffsets[currLine-1] < fileLen) || currLine == 1) {
                    currLine--;
                    redraw = 1;
                }
                break;
            case KEY_PGDOWN:
                if (currLine+(linesOnScreen/2) <= lastLine && lineOffsets[currLine+(linesOnScreen/2)] < fileLen) {
                    currLine += (linesOnScreen/2);
                    redraw = 1;
                } else goto end;
                break;
            case KEY_PGUP:
                if (currLine > (linesOnScreen/2) && lineOffsets[currLine-(linesOnScreen/2)] < fileLen) {
                    currLine -= (linesOnScreen/2);
                    redraw = 1;
                } else if (currLine <= (linesOnScreen/2)) {
                    currLine = 0;
                    redraw = 1;
                }
                break;
            case KEY_HOME: home:
                if (currLine != 0) {
                    currLine = 0;
                    redraw = 1;
                }
                break;
            case KEY_END: end:
                if (currLine != lastLine) {
                    currLine = lastLine;
                    redraw = 1;
                }
                break;
            case KEY_E: // e
                cont = 0;
                break;
            case KEY_F2:
                if (screenSize != 80*25) {
                    SetVideo25Lines();
                    SetCursorDisabled();
                    screenSize = 80*25;
                    redraw = 1;
                }
                break;
            case KEY_F5:
                if (screenSize != 80*50) {
                    SetVideo50Lines();
                    SetCursorDisabled();
                    screenSize = 80*50;
                    redraw = 1;
                }
                break;
            default:
                break;
        }
    }
}
// 400000 - 700000 Usermode Code (3mB)
// 700000 - 800000 Usermode Stack (1mB)
extern uint32_t create_user_child(uint32_t esp, uint32_t eip, uint32_t argc, ...);
void ProgramLoadTest(uint8_t *path, VOLINFO *vi) {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < 80*25; i++)
        vga_text[i] = 0x0f00;
    uint32_t successcount;
    uint8_t *diskReadBuf = (uint8_t *)0x400000;
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
    uint32_t res = create_user_child(0x800000, 0x400000, 0);
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
