#include "v86defs.h"
#include "print.h"
#include "dosfs/dosfs.h"
#include "stdint.h"

extern void *memcpy(void *restrict dest, const void *restrict src, uintptr_t n);

extern uint32_t _gpf_eax_save;
extern uint32_t _gpf_eflags_save;
extern uint16_t error_screen[80*50]; // defined in kernel.c
__attribute((__no_caller_saved_registers__))
extern void error_environment(); // defined in kernel.c
uint32_t numHead;
uint32_t secPerTrack;
uint32_t maxCylinder;
char useCHS = -1;
void Disk_SetupCHS() {
	union V86Regs_t regs;
	// Check for INT 13 Extensions support
	regs.h.ah = 0x41;
	regs.w.bx = 0x55AA;
	regs.h.dl = 0x80;
	V8086Int(0x13, &regs);
	// LBA supported if CF clear
	if (!(_gpf_eflags_save & 0x1)) {
		useCHS = 0;
	} else {
		FARPTR v86_entry = i386LinearToFp(v86DiskGetGeometry);
		enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
		// check CF for error
		if (_gpf_eflags_save & 0x1) {
			uint16_t *vga = error_screen;
			vga += printStr("Could not get Disk Geometry.", vga);
			error_environment();
			for(;;);
		}
		numHead = ((_gpf_eax_save & 0xff00) >> 8) + 1;
		secPerTrack = _gpf_eax_save & 0x3f;
		maxCylinder = ((_gpf_eax_save & 0xff0000) >> 16) | ((_gpf_eax_save & 0xc0) << 2);
		useCHS = 1;
	}
}
uint32_t DFS_ReadSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count) {
	// NOTE If the buffer provided is outside the 0x20000-0x2FE00 range,
	// the function will use that buffer for the Virtual 8086 process
	// and copy to the other buffer after
	uint8_t *v86buf = buffer;
	if ((uintptr_t)v86buf < 0x20000 || (uintptr_t)v86buf > 0x2FE00)
		v86buf = (uint8_t *)0x20000;

	// TODO This check should probably happen at the kernel level
	if (useCHS == -1) {
		Disk_SetupCHS();
	}

	// TODO Do error handling
	if (!useCHS) {
		// LBA Read
		v86disk_addr_packet.start_block = sector;
		v86disk_addr_packet.blocks = count;
		v86disk_addr_packet.transfer_buffer =
			(uintptr_t)v86buf & 0x000F |
			(((uintptr_t)v86buf & 0xFFFF0) << 12);
		union V86Regs_t regs;
		regs.h.ah = 0x42;
		FARPTR v86_entry = i386LinearToFp(v86DiskOp);
		enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
	} else {
        uint32_t tmp = sector / secPerTrack;
        uint32_t sec = (sector % (secPerTrack)) + 1;
        uint32_t head = tmp % numHead;
        uint32_t cyl = tmp / numHead;
		union V86Regs_t regs;
        regs.w.ax = 0x0201;
        regs.h.ch = cyl & 0xff;
        regs.h.cl = sec | ((cyl >> 2) & 0xc0);
        regs.h.dh = head;
        regs.h.dl = 0x80;
        regs.w.bx = (uintptr_t)v86buf & 0xFFFF;
        FARPTR v86_entry = i386LinearToFp(v86DiskReadCHS);
        enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
	}
	if (v86buf != buffer)
		memcpy(buffer, v86buf, count * SECTOR_SIZE);
	return 0;
}
uint32_t DFS_WriteSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count) {
	// NOTE If the buffer provided is outside the 0x20000-0x2FE00 range,
	// the function will use copy that buffer into the Virtual 8086 disk range
	uint8_t *v86buf = buffer;
	if ((uintptr_t)v86buf < 0x20000 || (uintptr_t)v86buf > 0x2FE00) {
		v86buf = (uint8_t *)0x20000;
		memcpy(v86buf, buffer, count * SECTOR_SIZE);
	}

	// TODO This check should probably happen at the kernel level
	if (useCHS == -1) {
		Disk_SetupCHS();
	}

	// TODO Do error handling
	if (!useCHS) {
		// LBA Write
		v86disk_addr_packet.start_block = sector;
		v86disk_addr_packet.blocks = count;
		v86disk_addr_packet.transfer_buffer =
			(uintptr_t)v86buf & 0x000F |
			(((uintptr_t)v86buf & 0xFFFF0) << 12);
		union V86Regs_t regs;
		regs.w.ax = 0x4300;
		FARPTR v86_entry = i386LinearToFp(v86DiskOp);
		enter_v86(0x8000, 0xFF00, FP_SEG(v86_entry), FP_OFF(v86_entry), &regs);
	} else {
		uint16_t *vga = error_screen;
		vga += printStr("CHS Write Unimplemented", vga);
		error_environment();
		for(;;);
	}
	return 0;
}

