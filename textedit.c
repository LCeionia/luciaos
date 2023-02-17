#include "progs.h"

#define MAXFILESIZE 0x300000 // 3MB
#define MAXLINES 100000
#define BLOCKSIZE 0x1000
#define BLOCKMASK 0xfff
#define BLOCKSHIFT 12
#define TOTALBLOCKS (MAXFILESIZE/BLOCKSIZE)
uintptr_t lineOffsets[MAXLINES] __attribute__((section(".textbss")));;
uintptr_t lineLengths[MAXLINES] __attribute__((section(".textbss")));;
uint8_t editedBlocks[TOTALBLOCKS] __attribute__((section(".textbss")));;
uint8_t fileBuffer[MAXFILESIZE]
    __attribute__((aligned(0x1000)))
    __attribute__((section(".textlatebss")));
void TextViewTest(char *path) {
    uint16_t *vga_text = (uint16_t *)0xb8000;
    uint32_t fileLen;
    {
        uint32_t err;
        dirent de;
        err = path_getinfo(path, &de);
        if (err) {
            vga_text += printStr("Error getting file info.", vga_text);
            kbd_wait();
            return;
        }
        fileLen = de.size;
        FILE file;
        err = file_open(&file, path, OPENREAD);
        if (err) {
            vga_text += printStr("Open Error: ", vga_text);
            printDword(err, vga_text);
            kbd_wait();
            return;
        }
        // file too large
        if (fileLen > MAXFILESIZE) {
            vga_text += printStr("File too large.", vga_text);
            kbd_wait();
            return;
        }
        if (file_seek(&file, 0)) {
            vga_text += printStr("Seek Error", vga_text);
            kbd_wait();
            return;
        }
        uint32_t bytesRead = file_read(&file, fileBuffer, fileLen);
        if (bytesRead < fileLen) {
            vga_text += printStr("Read Error: ", vga_text);
            printDword(err, vga_text);
            kbd_wait();
            return;
        }
    }
    uint32_t lastLine;
    {
        char nl;
        uint8_t c = 0x0A; // start with a pretend newline
        uint32_t line = -1; // start a pretend line behind
        uint32_t lineLen = 0;
        for (int32_t o = -1; o < (int32_t)fileLen; lineLen++, c = fileBuffer[++o]) {
            // newline
            if (c == 0x0A) {
                lineOffsets[++line] = o;
                lineLengths[line] = lineLen > 0 ? lineLen : 1;
                lineLen = 0;
            }
            // file too large
            if (line >= MAXLINES) {
                vga_text += printStr("File too large.", vga_text);
                kbd_wait();
                return;
            }
        }
        lineLengths[line] = lineLen;
        lastLine = line;
    }
    uint32_t currLine = 0;
    char cont = 1;
    uint32_t screenSize = 80*25;
    char redraw = 1;
    uint32_t linesOnScreen = 0;
    uint32_t cursorLine = 0;
    uint32_t cursorLineOffset = 0;
    char cursorChange = 0;
    for (;cont;) {
        if (cursorLine > lastLine) {
            cursorLine = lastLine;
            cursorLineOffset = lineLengths[lastLine] - 2;
            cursorChange = 1;
        }
        if (lineLengths[cursorLine+1] == 1 && cursorLineOffset == 0){}
        else if (cursorLineOffset >= lineLengths[cursorLine+1] - 1) {
            cursorLineOffset = 0;
            cursorLine++;
            cursorChange = 1;
        }

        if (redraw || cursorChange) {
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
                const char prnt[] = "Exit: F1";
                vga_text = &((uint16_t*)0xb8000)[80-sizeof(prnt)+1];
                vga_text += printStr((char*)prnt, vga_text);
            }
            for (vga_text = &((uint16_t*)0xb8000)[84]; vga_text < &((uint16_t*)0xb8000)[screenSize]; vga_text += 80)
                *(uint8_t*)vga_text = '|';
            vga_text = &((uint16_t*)0xb8000)[0];
            uint32_t screenLineOff = 6;
            uint32_t lineOffset = 0;
            uint8_t c = 0x0A; // start with a pretend newline
            uint32_t line = currLine - 1; // start a pretend line behind
            int32_t o = lineOffsets[currLine]; // the real or fake newline on previous line
            linesOnScreen = screenSize/80;
            for (; o < (int32_t)fileLen && vga_text < &((uint16_t*)0xb8000)[screenSize]; c = fileBuffer[++o]) {
                // newline
                if (c == 0x0A) {
                    vga_text = nextLine(vga_text,(uint16_t*)0xb8000);
                    line++;
                    lineOffset = 0;
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
                    screenLineOff = 6;
                    if (cursorLine == line && cursorLineOffset == lineOffset) {
                        ((uint8_t*)vga_text)[1] = 0xf0;
                    }
                    continue;
                } 
                *(uint8_t*)vga_text = c;
                if (cursorLine == line && cursorLineOffset == lineOffset) {
                    ((uint8_t*)vga_text)[1] = 0xf0;
                }
                lineOffset++;
                vga_text++;
                screenLineOff++;
                if (screenLineOff == 80) { // last char
                    vga_text += 6;
                    screenLineOff = 6;
                    linesOnScreen--;
                }
            }
            redraw = 0;
        }
        uint16_t key = get_scancode();
        union V86Regs_t regs;
        FARPTR v86_entry;
        switch (key & 0xff) {
            /*case KEY_DOWN: // down
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
                break;*/
            case KEY_DOWN:
                cursorLine++;
                cursorChange = 1;
                break;
            case KEY_UP:
                if (cursorLine > 0) cursorLine--;
                cursorChange = 1;
                break;
            case KEY_LEFT:
                if (cursorLineOffset > 0) cursorLineOffset--;
                cursorChange = 1;
                break;
            case KEY_RIGHT:
                cursorLineOffset++;
                cursorChange = 1;
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
            case KEY_F1:
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
