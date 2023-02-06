#pragma once
#include <stdint.h>

#include "interrupt.h"
#include "v86defs.h"
#include "dosfs/dosfs.h"

void V8086Int(uint8_t interrupt, union V86Regs_t *regs);

void SetVideo25Lines();
void SetVideo50Lines();
void SetCursorDisabled();

uint16_t *nextLine(uint16_t *p);

uint32_t OpenVol(VOLINFO *vi);
uint32_t OpenDir(uint8_t *path, VOLINFO *vi, DIRINFO *di);
void File83ToPath(char *src, char *path);
void GetFileList(DIRENT *entries, int32_t *entCount, VOLINFO *vi, DIRINFO *di);
