#include "helper.h"

uint16_t *nextLine(uint16_t *p, uint16_t *b) {
    uintptr_t v = (uintptr_t)p;
    return (uint16_t *)(v + (160 - ((v - (uintptr_t)b) % 160)));
}

void trimPath(char *path, char *buff, uint32_t maxLen) {
    int pathLen = 0;
    for (;path[pathLen];pathLen++);
    pathLen++;
    if (pathLen < maxLen) {
        for(int i = 0; i < pathLen; i++)
            buff[i] = path[i];
        return;
    }
    for (int i = 0; i < 3; i++)
        buff[i] = '.';
    for (int i = 3; i < maxLen; i++) {
        buff[i] = path[pathLen-maxLen+i];
    }
}

void V8086Int(uint8_t interrupt, union V86Regs_t *regs) {
    // Edit the v8086 code with the interrupt
    // Writing 4 bytes to ensure proper code
    *(uint32_t*)v86Interrupt = 0x30CD00CD | (interrupt << 8);
    FARPTR v86_entry = i386LinearToFp(v86Interrupt);
    enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), regs);
}

void SetVideo25Lines() {
    union V86Regs_t regs;
    regs.w.ax = 0x1114; // 80x25 mode
    regs.w.bx = 0x0000;
    V8086Int(0x10, &regs);
}
void SetVideo50Lines() {
    union V86Regs_t regs;
    regs.w.ax = 0x1112; // 80x50 mode
    regs.w.bx = 0x0000;
    V8086Int(0x10, &regs);
}
void SetCursorDisabled() {
    union V86Regs_t regs;
    regs.w.ax = 0x0100; // set cursor
    regs.w.cx = 0x3F00; // disabled
    V8086Int(0x10, &regs);
}

void GetFileList(DIR *dir, dirent *entries, int32_t *entCount, int32_t maxEntries) {
    for ((*entCount) = 0; *entCount < maxEntries && !dir_nextentry(dir, &entries[*entCount]); (*entCount)++);
}
