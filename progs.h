#pragma once
#include <stdint.h>

#include "print.h"
#include "kbd.h"
#include "v86defs.h"
#include "helper.h"
#include "file.h"

void HexEditor(char *path, dirent *de);
void TextViewTest(char *path, dirent *de);
void ProgramLoadTest(char *path, dirent *de);
