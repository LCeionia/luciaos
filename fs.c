#include "fs.h"

struct FsType {
	uint32_t type_id;
	int (*init_func)(filesystem *, char);
	// Not yet decided
	char (*detect_func)();
};

// TODO Get these dynamically somehow
int InitDosFs(filesystem *fs, char partition);
char DetectDosPart();

struct FsType SupportedFilesystems[] = {
	{
		.type_id =  0xD05,
		.init_func = InitDosFs,
		.detect_func = DetectDosPart
	}
};

#define MAXFS (0x20000/sizeof(filesystem))
filesystem (*const ActiveFilesystems)[MAXFS] = (filesystem (* const)[MAXFS])0x240000;

// TODO Replace with something better
uint8_t ActiveFsId;

// TODO Make functions and just use those instead
int MakeSystemVolume(uint8_t sysPartition) {
	filesystem *sys = &(*ActiveFilesystems)[0];
	SupportedFilesystems[0].init_func(sys, sysPartition);
	sys->type = SupportedFilesystems[0].type_id;
	sys->id = 0;
	ActiveFsId = 0;
	return 0;
}
