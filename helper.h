#pragma once
#include <stdint.h>

#include "interrupt.h"
#include "v86defs.h"
#include "file.h"

void V8086Int(uint8_t interrupt, union V86Regs_t *regs);

void SetVideo25Lines();
void SetVideo50Lines();
void SetCursorDisabled();

uint16_t *nextLine(uint16_t *p, uint16_t *b);
void trimPath(char *path, char *buff, uint32_t maxLen);

void GetFileList(DIR *dir, dirent *entries, int32_t *entCount, int32_t maxEntries);
