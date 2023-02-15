#pragma once
#include <stdint.h>

void InitDisk();

uint32_t Disk_ReadSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count);
uint32_t Disk_WriteSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count);
