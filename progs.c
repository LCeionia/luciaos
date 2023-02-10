#include "progs.h"
#include "helper.h"
#include "kbd.h"

void HexViewTest(uint8_t *path, VOLINFO *vi) {
    uint32_t err;
    uint16_t *vga_text = (uint16_t *)0xb8000;
    uint32_t screenSize = 80*25;
    uint8_t *scratch = (uint8_t *)0x20000;

    const uint32_t blockSize = 0x10000; // 64K
    const uint32_t blockMask = ~0xffff; // block = (offset & blockMask)
    const uint32_t blockShift = 16; // blockSize = 1 << blockShift
    const uint32_t maxFileSize = 0x80000000; // 2GB
    const uint32_t totalBlocks = maxFileSize / blockSize;
    // TODO This hackish code means we should really be loading these
    // programs rather than having them all be in the kernel itself
    uint16_t (*writtenMap)[totalBlocks] =
        (uint16_t (*)[totalBlocks])0x400000;
    uint32_t (*blockLenMap)[totalBlocks] =
        (uint32_t (*)[totalBlocks])((uintptr_t)writtenMap + sizeof(*writtenMap));
    uint8_t (*writeStore)[blockSize] =
        (uint8_t (*)[blockSize])((uintptr_t)blockLenMap + sizeof(*blockLenMap));
    for (int i = 0; i < totalBlocks; i++)
        (*writtenMap)[i] = 0;
    uint8_t *screenBuff = *writeStore;
    // First two blocks are screen buffer
    uint32_t nextFreeBlock = 2;

    FILEINFO fi;
    vga_text = (uint16_t *)0xb8000;
    for (int i = 0; i < 80*50; i++)
        vga_text[i] = 0x0f00;
    err = DFS_OpenFile(vi, path, DFS_READ | DFS_WRITE, scratch, &fi);
    if (err) {
        vga_text += printStr("Open Error: ", vga_text);
        printDword(err, vga_text);
        kbd_wait();
        return;
    }
    if (fi.filelen == 0) {
        vga_text += printStr("File ", vga_text);
        vga_text += printStr((char*)path, vga_text);
        vga_text += printStr(" has no data.", vga_text);
        kbd_wait();
        return;
    }
    if (fi.filelen > maxFileSize) {
        vga_text += printStr("File ", vga_text);
        vga_text += printStr((char*)path, vga_text);
        vga_text += printStr(" is too large (> 2GB).", vga_text);
        kbd_wait();
        return;
    }
    uint32_t drawOffset = 0, lastDrawOffset = -1;
    char cont = 1;
    uint32_t byteCount = 16*24, lastByteCount = 0;
    // Cursor offset from *drawOff*
    int32_t cursorScreenOff = 0, lastCursorScreenOff = -1;
    // Pointer to last cursor in *VGA mem*
    uint8_t *lastCursorScreenPtr = (uint8_t*)0xb8000;
    char cursorNibble = 1, lastCursorNibble = 0;
    char cursorRedraw = 1;
    char reread = 1, redraw = 1;
    uint8_t *currBuff = 0, *nextBuff = 0;
    uint32_t currBuffLength, nextBuffLength, totalBuffLength;
    uint16_t currLoadedBlock = -1, nextLoadedBlock = -1;
    char currInMap = 0, nextInMap = 0;
    uint32_t changeOffset = -1, lastChangeOffset = -1;
    char fileChanged = 0;
    for (;cont;) {
        // Check if we need to scroll screen for cursor
        // Should never be more than 16 away, if it is,
        // things will be caught by sanity checks.
        // Scroll Back
        if (cursorScreenOff < 0) {
            if (drawOffset - 16 < fi.filelen)
                drawOffset -= 16;
            cursorScreenOff += 16;
        }
        // Scroll Forward
        if (cursorScreenOff >= byteCount) {
            if (drawOffset + 16 < fi.filelen)
                drawOffset += 16;
            cursorScreenOff -= 16;
        }

        // Sanity checks
        if (cursorScreenOff >= byteCount)
            cursorScreenOff = byteCount - 1;
        if (cursorScreenOff + drawOffset >= fi.filelen)
            cursorScreenOff = fi.filelen - drawOffset - 1;
        if (cursorScreenOff < 0) cursorScreenOff = 0;

        if (cursorNibble != lastCursorNibble)
            cursorRedraw = 1;
        if (cursorScreenOff != lastCursorScreenOff)
            cursorRedraw = 1;
        if (cursorRedraw) {
            const uint32_t hexViewScreenOff = 10 * 2;
            const uint32_t asciiViewScreenOff = 61 * 2;
            uint16_t *screen = (uint16_t*)0xb8000;
            // Byte draw starts on first line
            uint16_t *line = &screen[80 * (1 + (cursorScreenOff >> 4))];
            uint8_t *cursorPtr = (uint8_t*)line;
            if (cursorNibble == 0) {
                // Each byte takes 3 chars on screen
                cursorPtr += hexViewScreenOff + (cursorScreenOff & 0xF) * 3 * 2;
                if ((cursorScreenOff & 0xF) > 7)
                    cursorPtr += 2;
                // Low nibble
                cursorPtr += 2;
            } else if (cursorNibble == 1) {
                // Each byte takes 3 chars on screen
                cursorPtr += hexViewScreenOff + (cursorScreenOff & 0xF) * 3 * 2;
                if ((cursorScreenOff & 0xF) > 7)
                    cursorPtr += 2;
            // ASCII area
            } else {
                // Each byte takes 1 char on screen
                cursorPtr += asciiViewScreenOff + (cursorScreenOff & 0xF) * 2;
            }
            // We want the color byte, not char
            cursorPtr++;
            if (cursorPtr != lastCursorScreenPtr) {
                *lastCursorScreenPtr = 0x0f;
                *cursorPtr = 0xf0;
                lastCursorScreenPtr = cursorPtr;
            }
            lastCursorScreenOff = cursorScreenOff;
            lastCursorNibble = cursorNibble;
            cursorRedraw = 0;
        }

        if ((drawOffset & blockMask) != (lastDrawOffset & blockMask)) {
            lastDrawOffset = (drawOffset & blockMask);
            reread = 1;
            redraw = 1;
        }
        if (drawOffset != lastDrawOffset) {
            lastDrawOffset = drawOffset;
            redraw = 1;
        }
        if (byteCount != lastByteCount) {
            lastByteCount = byteCount;
            redraw = 1;
        }
        if (changeOffset != lastChangeOffset) {
            redraw = 1;
            lastChangeOffset = changeOffset;
        }
        // Check for changes to block
        if (changeOffset != -1) {
            char isCurr = changeOffset < blockSize;
            char changeInMap = isCurr ? currInMap : nextInMap;
            uint8_t *changedBlock = isCurr ? currBuff : nextBuff;
            // If changes are in map we don't need to do anything
            if (!changeInMap) {
                uint16_t loadedBlock = isCurr ? currLoadedBlock : nextLoadedBlock;
                uint16_t storeBlockIdx = (*writtenMap)[loadedBlock];
                // Add a new map entry
                if (!storeBlockIdx) {
                    (*writtenMap)[loadedBlock] = storeBlockIdx = nextFreeBlock++;
                    (*blockLenMap)[loadedBlock] = isCurr ? currBuffLength : nextBuffLength;
                }
                uint8_t *storeBlock = writeStore[storeBlockIdx];
                for (int i = 0; i < blockSize; i++)
                    storeBlock[i] = changedBlock[i];
                // Move the current buffer to map
                if (isCurr) { currBuff = storeBlock; currInMap = 1; }
                else { nextBuff = storeBlock; nextInMap = 1; }
            }
            changeOffset = -1;
            fileChanged = 1;
            redraw = 1;
        }

        if (reread) {
            // TODO We should probably check if we already have
            // the block to read in nextBuff from previous stuff
            // We are in a modified block
            uint16_t newBlock = drawOffset >> blockShift;
            if ((*writtenMap)[newBlock]) {
                currBuff = writeStore[(*writtenMap)[newBlock]];
                currBuffLength = (*blockLenMap)[newBlock];
                totalBuffLength = currBuffLength;
                currInMap = 1;
            // We are in an unmodified block
            } else {
                uint32_t blockOffset = drawOffset & blockMask;
                vga_text = &((uint16_t*)0xb8000)[80];
                DFS_Seek(&fi, blockOffset, scratch);
                if (fi.pointer != blockOffset) {
                    vga_text += printStr("Seek Error", vga_text);
                    kbd_wait();
                    return;
                }
                err = DFS_ReadFile(&fi, scratch, screenBuff, &currBuffLength, blockSize);
                if (err && err != DFS_EOF) {
                    vga_text += printStr("Read Error: ", vga_text);
                    printDword(err, vga_text);
                    kbd_wait();
                    return;
                }
                currBuff = screenBuff;
                totalBuffLength = currBuffLength;
                currInMap = 0;
            }
            currLoadedBlock = newBlock;
            reread = 0;
            nextInMap = 0;
        }
        // We will be drawing stuff in the next block
        // TODO This would be better served with nice functions!
        if (currBuffLength == blockSize && (drawOffset & ~blockMask) > (blockSize - byteCount)) {
            // TODO We should probably check if we already have
            // the next block in currBuff from previous stuff
            // Next is a modified block
            uint16_t newBlock = (drawOffset >> blockShift) + 1;
            if (nextLoadedBlock != newBlock) {
                if ((*writtenMap)[newBlock]) {
                    nextBuffLength = (*blockLenMap)[newBlock];
                    nextBuff = writeStore[(*writtenMap)[newBlock]];
                    nextInMap = 1;
                // Next is an unmodified block
                } else {
                    uint32_t blockOffset = (drawOffset & blockMask) + blockSize;
                    vga_text = &((uint16_t*)0xb8000)[80];
                    DFS_Seek(&fi, blockOffset, scratch);
                    if (fi.pointer != blockOffset) {
                        vga_text += printStr("Seek Error", vga_text);
                        kbd_wait();
                        return;
                    }
                    err = DFS_ReadFile(&fi, scratch, &screenBuff[blockSize], &nextBuffLength, blockSize);
                    if (err && err != DFS_EOF) {
                        vga_text += printStr("Read Error: ", vga_text);
                        printDword(err, vga_text);
                        kbd_wait();
                        return;
                    }
                    nextBuff = &screenBuff[blockSize];
                    nextInMap = 0;
                }
                nextLoadedBlock = newBlock;
            }
            totalBuffLength = currBuffLength + nextBuffLength;
        }
        if (redraw) {
            vga_text = (uint16_t *)0xb8000;
            vga_text += printStr((char*)path, vga_text);
            vga_text += printChar(fileChanged ? '*' : ' ', vga_text);
            {
                const char prnt[] = "Scroll: Up/Down PgUp/PgDown Home/End             Exit: F1";
                vga_text = &((uint16_t*)0xb8000)[80-sizeof(prnt)];
                vga_text += printStr((char*)prnt, vga_text);
            }
            vga_text = &((uint16_t*)0xb8000)[80];
            uint32_t offsetInBuff = drawOffset & ~blockMask;
            uint8_t *drawFromBuff = &currBuff[offsetInBuff];
            uint32_t maxDrawLen = totalBuffLength - (drawOffset & ~blockMask);
            for (int i = 0; i < maxDrawLen && i < byteCount; i += 16) {
                // Blocks are blockSize aligned, so 16-byte alignment should work
                if (i + offsetInBuff >= blockSize) {
                    drawFromBuff = &nextBuff[-i];
                    offsetInBuff = 0;
                }
                vga_text += printDword(i + drawOffset, vga_text);
                vga_text += printChar(' ', vga_text);
                vga_text += printChar(' ', vga_text);
                for (uint32_t j = 0; j < 16; j++) {
                    if (i + j < maxDrawLen)
                        vga_text += printByte(drawFromBuff[i + j], vga_text);
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
                    if (i + j < maxDrawLen)
                        vga_text += printChar(drawFromBuff[i + j], vga_text);
                    else vga_text += printChar(' ', vga_text);
                }
                vga_text += printChar('|', vga_text);
                vga_text = nextLine(vga_text);
            }
            // Clear remainder of screen
            for (;vga_text < &((uint16_t*)0xb8000)[screenSize]; vga_text++)
                *vga_text = 0x0f00;
            redraw = 0;
        }
        uint16_t key = get_scancode();
        union V86Regs_t regs;
        FARPTR v86_entry;
        char try_edit = 0;
        switch (key & 0xff) {
            case KEY_DOWN:
                // Stay in file
                if ((cursorScreenOff + 16 + drawOffset) < fi.filelen)
                    cursorScreenOff += 16;
                break;
            case KEY_UP:
                // Stay in file
                if ((uint32_t)(cursorScreenOff - 16 + drawOffset) < fi.filelen)
                    cursorScreenOff -= 16;
                break;
            case KEY_LEFT:
                if (cursorNibble == 0) { cursorNibble = 1; break; }
                // Exiting text area
                else if (cursorNibble == 2 && (cursorScreenOff & 0xF) == 0) {
                    cursorNibble = 0;
                    cursorScreenOff |= 0xF;
                // Stay in file
                } else if ((cursorScreenOff - 1 + drawOffset) < fi.filelen) {
                    cursorScreenOff--;
                    if (cursorNibble == 1) cursorNibble = 0;
                }
                break;
            case KEY_RIGHT:
                if (cursorNibble == 1) { cursorNibble = 0; break; }
                // Entering text area
                else if (cursorNibble == 0 && (cursorScreenOff & 0xF) == 0xF) {
                    cursorNibble = 2;
                    cursorScreenOff &= ~0xF;
                // Stay in file
                } else if ((cursorScreenOff + 1 + drawOffset) < fi.filelen) {
                    cursorScreenOff++;
                    if (cursorNibble == 0) cursorNibble = 1;
                }
                break;
            case KEY_PGDOWN:
                if (drawOffset + byteCount < fi.filelen)
                    drawOffset += byteCount;
                else if ((fi.filelen / byteCount) * byteCount > drawOffset)
                    drawOffset = (fi.filelen / byteCount) * byteCount;
                break;
            case KEY_PGUP:
                if (drawOffset - byteCount < fi.filelen)
                    drawOffset -= byteCount;
                else drawOffset = 0;
                break;
            case KEY_HOME:
                cursorScreenOff &= ~0xF;
                if (cursorNibble == 0) cursorNibble = 1;
                break;
            case KEY_END:
                cursorScreenOff |= 0xF;
                if (cursorNibble == 1) cursorNibble = 0;
                break;
            case KEY_F1:
                cont = 0;
                break;
            case KEY_F3: // start of file
                drawOffset = 0;
                break;
            case KEY_F4: // end of file
                if ((fi.filelen / byteCount) * byteCount > drawOffset)
                    drawOffset = (fi.filelen / byteCount) * byteCount;
                break;
            case KEY_F6: // TODO write file
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
                try_edit = 1;
                break;
        }
        if (!try_edit) continue;
        // Check if in range
        char k = key >> 8; // ASCII portion
        char v;
        if (cursorNibble == 2 && k >= 0x20 && k <= 0x7E) v = k;
        else if (k >= '0' && k <= '9')
            v = k - '0';
        else if (k >= 'a' && k <= 'f')
            v = k - 'a' + 0xA;
        else if (k >= 'A' && k <= 'F')
            v = k - 'A' + 0xA;
        else continue;
        // Change buffer at cursor
        uint32_t screenOffsetInBuff = drawOffset & ~blockMask;
        changeOffset = screenOffsetInBuff + cursorScreenOff;
        char isCurr = changeOffset < blockSize;
        uint8_t *changeBuff = isCurr ? currBuff : nextBuff;
        uint32_t changeOffsetInBuff = isCurr ? changeOffset : changeOffset - blockSize;
        if (cursorNibble == 0)
            changeBuff[changeOffsetInBuff] = (changeBuff[changeOffsetInBuff] & 0xF0) | v;
        else if (cursorNibble == 1)
            changeBuff[changeOffsetInBuff] = (changeBuff[changeOffsetInBuff] & 0x0F) | (v << 4);
        else changeBuff[changeOffsetInBuff] = v;
        redraw = 1;
    }
    if (!fileChanged) return;
    vga_text = (uint16_t*)0xb8000;
    vga_text += printStr((char*)path, vga_text);
    vga_text += printChar(fileChanged ? '*' : ' ', vga_text);
    vga_text += printChar(' ', vga_text);
    vga_text += printStr("Save changes to file? (Y/N)", vga_text);
    for (;vga_text < &((uint16_t*)0xb8000)[80];vga_text++)
        *vga_text = 0x0f00;
    uint16_t key=0;
    for(;(key & 0xff) != KEY_N && (key & 0xff) != KEY_Y;key = get_scancode());
    if ((key & 0xff) != KEY_Y) return;
    // Write changes
    for (int i = 0; i < totalBlocks && (i << blockShift) < fi.filelen; i++) {
        // No change in current block
        uint16_t blockIdx = (*writtenMap)[i];
        uint32_t blockLen = (*blockLenMap)[i];
        if (!blockIdx) continue;
        // Write block to file
        uint32_t successcount;
        uint32_t blockOff = i << blockShift;
        DFS_Seek(&fi, blockOff, scratch);
        if (fi.pointer != blockOff) {
            vga_text = (uint16_t*)0xb8000;
            vga_text += printStr("Seek Error ", vga_text);
            kbd_wait();
            return;
        }
        uint32_t err = DFS_WriteFile(&fi, scratch, writeStore[blockIdx], &successcount, blockLen);
        if (successcount < blockLen || err) {
            vga_text = (uint16_t*)0xb8000;
            vga_text += printStr("Write Error ", vga_text);
            kbd_wait();
            return;
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
    vga_text = nextLine(vga_text);
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
