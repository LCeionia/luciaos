#include "progs.h"

void HexViewTest(uint8_t *path, VOLINFO *vi) {
    uint32_t err;
    uint16_t *vga_text = (uint16_t *)0xb8000;
    uint8_t *scratch = (uint8_t *)0x20000;
    FILEINFO fi;
    err = DFS_OpenFile(vi, path, DFS_READ, scratch, &fi);
    if (err) {
        vga_text += printStr("Open Error: ", vga_text);
        printDword(err, vga_text);
        return;
    }
    uint32_t successcount;
    uint32_t readOffset = 0, lastReadOffset = -1;
    char cont = 1;
    uint32_t byteCount = 16*24, lastByteCount;
    uint32_t screenSize = 80*25;
    char reread;
    for (;cont;) {
        uint8_t diskReadBuf[byteCount];
        if (readOffset != lastReadOffset) {
            lastReadOffset = readOffset;
            reread = 1;
        }
        if (byteCount != lastByteCount) {
            lastByteCount = byteCount;
            reread = 1;
        }
        if (reread) {
            vga_text = (uint16_t *)0xb8000;
            for (int i = 0; i < screenSize; i++)
                vga_text[i] = 0x0f00;
            vga_text += printStr((char*)path, vga_text);
            {
                const char prnt[] = "Scroll: Up/Down PgUp/PgDown Home/End             Exit: E ";
                vga_text = &((uint16_t*)0xb8000)[80-sizeof(prnt)];
                vga_text += printStr((char*)prnt, vga_text);
            }
            vga_text = &((uint16_t*)0xb8000)[80];
            DFS_Seek(&fi, readOffset, scratch);
            if (fi.pointer != readOffset) {
                vga_text += printStr("Seek Error", vga_text);
                return;
            }
            err = DFS_ReadFile(&fi, scratch, diskReadBuf, &successcount, byteCount);
            if (err && err != DFS_EOF) {
                vga_text += printStr("Read Error: ", vga_text);
                printDword(err, vga_text);
                return;
            }
            for (uint32_t i = 0; i < successcount && i < byteCount; i += 16) {
                vga_text += printDword(i + readOffset, vga_text);
                vga_text += printChar(' ', vga_text);
                vga_text += printChar(' ', vga_text);
                for (uint32_t j = 0; j < 16; j++) {
                    if (i + j < successcount)
                        vga_text += printByte(diskReadBuf[i + j], vga_text);
                    else {
                        vga_text += printChar(' ', vga_text);
                        vga_text += printChar(' ', vga_text);
                    }
                    vga_text += printChar(' ', vga_text);
                    if (j == 7)
                        vga_text += printChar(' ', vga_text);
                }
                vga_text += printChar(' ', vga_text);
                vga_text += printChar('|', vga_text);
                for (uint32_t j = 0; j < 16; j++) {
                    if (i + j < successcount)
                        vga_text += printChar(diskReadBuf[i + j], vga_text);
                    else vga_text += printChar(' ', vga_text);
                }
                vga_text += printChar('|', vga_text);
                vga_text = nextLine(vga_text);
            }
            reread = 0;
        }
        uint16_t key = get_scancode();
        union V86Regs_t regs;
        FARPTR v86_entry;
        switch (key & 0xff) {
            case KEY_DOWN: // down
                if ((readOffset + byteCount) < fi.filelen)
                    readOffset += byteCount;
                else goto end;
                break;
            case KEY_UP: // up
                if ((readOffset - byteCount) < fi.filelen)
                    readOffset -= byteCount;
                else goto home;
                break;
            case KEY_PGDOWN:
                if ((readOffset + (byteCount*4)) < fi.filelen)
                    readOffset += (byteCount*4);
                else goto end;
                break;
            case KEY_PGUP:
                if ((readOffset - (byteCount*4)) < fi.filelen)
                    readOffset -= (byteCount*4);
                else goto home;
                break;
            case KEY_HOME: home:
                readOffset = 0;
                break;
            case KEY_END: end:
                if ((fi.filelen / byteCount) * byteCount > readOffset)
                    readOffset = (fi.filelen / byteCount) * byteCount;
                break;
            case KEY_E: // e
                cont = 0;
                break;
            case KEY_F2:
                if (byteCount != 16*24) {
                    SetVideo25Lines();
                    SetCursorDisabled();
                    screenSize = 80*25;
                    byteCount = 16*24;
                }
                break;
            case KEY_F5:
                if (byteCount != 16*49) {
                    SetVideo50Lines();
                    SetCursorDisabled();
                    screenSize = 80*50;
                    byteCount = 16*49;
                }
                break;
            default:
                break;
        }
    }
}
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
            vga_text += printStr((char*)path, vga_text);
            vga_text += 2;
            vga_text += printStr("Line: ", vga_text);
            vga_text += printDec(currLine, vga_text);
            vga_text += printChar('/', vga_text);
            vga_text += printDec(lastLine, vga_text);
            vga_text += printStr(" Scroll: Up/Down PgUp/PgDown Home/End", vga_text);
            {
                const char prnt[] = "Exit: E ";
                vga_text = &((uint16_t*)0xb8000)[80-sizeof(prnt)];
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
                    vga_text = nextLine(vga_text);
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
