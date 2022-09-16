#pragma once
#include <stdint.h>

__attribute((__no_caller_saved_registers__))
void printByte(uint8_t v, uint16_t *buff);
__attribute((__no_caller_saved_registers__))
void printWord(uint16_t v, uint16_t *buff);
__attribute((__no_caller_saved_registers__))
void printDword(uint32_t v, uint16_t *buff);
