#pragma once
#include <stdint.h>

extern void v86Test();
extern void v86TransFlag();
extern void v86GfxMode();
extern void v86TextMode();
extern void v86DiskRead();
extern void v86VideoInt();

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

extern void enter_v86(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip, union V86Regs_t *regs);

struct __attribute((__packed__)) Int13DiskPacket_t {
    uint8_t size; // 0x10
    uint8_t reserved; // 0x00
    uint16_t blocks;
    uint32_t transfer_buffer; // 0x2300:0000
    uint64_t start_block;
};

extern struct Int13DiskPacket_t v86disk_addr_packet;
