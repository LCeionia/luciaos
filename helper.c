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

uint32_t OpenVol(VOLINFO *vi) {
    uint8_t *diskReadBuf = (uint8_t *)0x20000;
    uint8_t pactive, ptype;
    uint32_t pstart, psize;
    pstart = DFS_GetPtnStart(0, diskReadBuf, 0, &pactive, &ptype, &psize);
    if (pstart == -1) return -1;
    return DFS_GetVolInfo(0, diskReadBuf, pstart, vi);
}
uint32_t OpenDir(uint8_t *path, VOLINFO *vi, DIRINFO *di) {
    uint8_t *diskReadBuf = (uint8_t *)0x20000;
    di->scratch = diskReadBuf;
    return DFS_OpenDir(vi, path, di);
}

void File83ToPath(char *src, char *path) {
    uint8_t tmp, trailingSpace;
    for (trailingSpace=0, tmp = 0; tmp < 8 && src[tmp]; tmp++) {
        path[tmp] = src[tmp];
        if (src[tmp] == ' ') trailingSpace++;
        else trailingSpace = 0;
    }
    tmp -= trailingSpace;
    path[tmp++] = '.';
    trailingSpace = 0;
    for (int i = 8; i < 11 && src[i]; i++, tmp++) {
        path[tmp] = src[i];
        if (src[i] == ' ') trailingSpace++;
        else trailingSpace = 0;
    }
    tmp -= trailingSpace;
    if (trailingSpace == 3) tmp--;
    path[tmp] = 0;
}

void GetFileList(DIRENT *entries, int32_t *entCount, int32_t maxEntries, VOLINFO *vi, DIRINFO *di) {
    uint8_t *diskReadBuf = (uint8_t *)0x20000;
    DIRENT de;
    int32_t fileCount = 0;
    while (!DFS_GetNext(vi, di, &de)) {
        if (de.name[0]) {
            uint8_t *d = (uint8_t*)&entries[fileCount];
            uint8_t *s = (uint8_t*)&de;
            for (int i = 0; i < sizeof(DIRENT); i++)
                d[i] = s[i];
            fileCount++;
        }
        if (fileCount >= maxEntries) break;
    }
    *entCount = fileCount;
}
