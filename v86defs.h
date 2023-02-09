#pragma once
#include <stdint.h>

// Labels of v8086 programs 
// TODO Remove these and use
// a single define for location?
extern void v86Test();
extern void v86TransFlag();
extern void v86Interrupt();
extern void v86TextMode();
extern void v86DiskOp();
extern void v86DiskGetGeometry();
extern void v86DiskReadCHS();

union __attribute((__packed__)) V86Regs_t {
    struct dword_regs { 
        uint32_t edi;
        uint32_t esi;
        uint32_t ebx;
        uint32_t edx;
        uint32_t ecx;
        uint32_t eax;
    } d;
    struct word_regs { 
        uint16_t di, _upper_di;
        uint16_t si, _upper_si;
        uint16_t bx, _upper_bx;
        uint16_t dx, _upper_dx;
        uint16_t cx, _upper_cx;
        uint16_t ax, _upper_ax;
    } w;
    struct byte_regs { 
        uint16_t di, _upper_di;
        uint16_t si, _upper_si;
        uint8_t bl, bh;
        uint16_t _upper_bx;
        uint8_t dl, dh;
        uint16_t _upper_dx;
        uint8_t cl, ch;
        uint16_t _upper_cx;
        uint8_t al, ah;
        uint16_t _upper_ax;
    } h;
};

extern uint32_t enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip, union V86Regs_t *regs);

void V8086Int(uint8_t interrupt, union V86Regs_t *regs);

struct __attribute((__packed__)) Int13DiskPacket_t {
    uint8_t size; // 0x10
    uint8_t reserved; // 0x00
    uint16_t blocks;
    uint32_t transfer_buffer; // 0x2300:0000
    uint64_t start_block;
};

extern struct Int13DiskPacket_t v86disk_addr_packet;

/* Real Mode helper macros */
/* segment:offset pair */
typedef uint32_t FARPTR;

/* Make a FARPTR from a segment and an offset */
#define MK_FP(seg, off)  ((FARPTR) (((uint32_t) (seg) << 16) | (uint16_t) (off)))

/* Extract the segment part of a FARPTR */
#define FP_SEG(fp)        (((FARPTR) fp) >> 16)

/* Extract the offset part of a FARPTR */
#define FP_OFF(fp)        (((FARPTR) fp) & 0xffff)

/* Convert a segment:offset pair to a linear address */
#define FP_TO_LINEAR(seg, off) ((void*)(uintptr_t)((((uint32_t)seg) << 4) + ((uint32_t)off)))

#define EFLAG_IF ((uint32_t)1 << 9)
#define EFLAG_VM ((uint32_t)1 << 17)

FARPTR i386LinearToFp(void *ptr);

void ensure_v86env();
