#include "fs.h"
#include "disk.h"

struct FsType {
	uint32_t type_id;
	int (*init_func)(filesystem *, uint32_t);
	// Not yet decided
	char (*detect_func)(uint32_t);
};

// TODO Get these dynamically somehow
int InitDosFs(filesystem *fs, uint32_t start_sector);
char DetectDosPart(uint32_t start_sector);

struct FsType SupportedFilesystems[] = {
	{
		.type_id =  0xD05,
		.init_func = InitDosFs,
		.detect_func = DetectDosPart
	}
};

#define MAXFS 255
filesystem *ActiveFilesystems = (filesystem *)0x240000;

uint8_t ActiveFsIdx;

filesystem *GetFilesystem(uint8_t idx) {
	if (idx >= MAXFS) return 0;
	filesystem *fs = &ActiveFilesystems[idx];
	return fs->type != 0 ? fs : 0;
}

filesystem *GetActiveFilesystem() {
	return &ActiveFilesystems[ActiveFsIdx];
}
uint8_t GetActiveFilesystemId() {
	return ActiveFsIdx;
}

filesystem *SetActiveFilesystem(uint8_t idx) {
	if (idx >= MAXFS) return 0;
	filesystem *fs = &ActiveFilesystems[idx];
	if (fs->type == 0) return 0;
	ActiveFsIdx = idx;
	return fs;
}

void ActiveFilesystemBitmap(char *bitmap) {
	for (int i = 0; i < 256; i++)
		bitmap[i] = ActiveFilesystems[i].type != 0;
}

struct PartTableEntry_t {
	uint8_t attr;
	uint8_t start_chs0;
	uint8_t start_chs1;
	uint8_t start_chs2;
	uint8_t type;
	uint8_t end_chs0;
	uint8_t end_chs1;
	uint8_t end_chs2;
	uint32_t start_lba;
	uint32_t num_sectors;
};
typedef struct MBR_t {
	char boot_code[0x1B8];
	uint32_t signature;
	uint16_t reserved;
	struct PartTableEntry_t partTable[4];
	uint16_t boot_sig;
} __attribute__((__packed__)) MBR_t;

// TODO Just check for this
uint8_t BootPartition;
uint8_t NextAvailableFilesystem;

MBR_t SysMbr;

void MakeMBRPartitions() {
	// Get MBR
	MBR_t *mbr = &SysMbr;
	Disk_ReadSector(0, (uint8_t*)mbr, 0, 1);

	// Scan partitions
	for (int p = 0; p < 4; p++) {
		if (p == BootPartition) continue;
		if (mbr->partTable[p].type == 0) continue;
		uint32_t partStart = mbr->partTable[p].start_lba;
		if (partStart == 0) continue;
		// Scan supported filesystems
		filesystem *sys = &ActiveFilesystems[NextAvailableFilesystem];
		for (int i = 0; i < sizeof(SupportedFilesystems)/sizeof(struct FsType); i++) {
			if (!SupportedFilesystems[i].detect_func(partStart)) continue;
			SupportedFilesystems[i].init_func(sys, partStart);
			sys->type = SupportedFilesystems[i].type_id;
			ActiveFsIdx = NextAvailableFilesystem;
			NextAvailableFilesystem++;
			break;
		}
	}
}

int MakeSystemVolume(uint8_t bootPartition) {
	// Clear out filesystem area
	for (int i = 0; i < (sizeof(filesystem)*MAXFS)/sizeof(uint32_t);i++)
		((uint32_t*)ActiveFilesystems)[i] = 0;

	BootPartition = bootPartition;
	NextAvailableFilesystem = 1;
	// Get MBR
	MBR_t *mbr = &SysMbr;
	Disk_ReadSector(0, (uint8_t*)mbr, 0, 1);
	// Get boot partition sector
	uint32_t sys_sector = mbr->partTable[bootPartition].start_lba;

	// Scan supported filesystems
	filesystem *sys = &ActiveFilesystems[0];
	for (int i = 0; i < sizeof(SupportedFilesystems)/sizeof(struct FsType); i++) {
		asm volatile("xchg %bx,%bx");
		if (!SupportedFilesystems[i].detect_func(sys_sector)) continue;
		SupportedFilesystems[i].init_func(sys, sys_sector);
		sys->type = SupportedFilesystems[i].type_id;
		ActiveFsIdx = 0;
		return 0;
	}

	// Init Failed
	return -1;
}
