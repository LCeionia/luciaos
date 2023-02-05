#include <stdint.h>

uintptr_t printByte(uint8_t v, uint16_t *buff);
uintptr_t printWord(uint16_t v, uint16_t *buff);
uintptr_t printDword(uint32_t v, uint16_t *buff);
uintptr_t printStr(char *v, uint16_t *buff);
uintptr_t printDec(uint32_t v, uint16_t *buff);
uintptr_t printChar(char v, uint16_t *buff);
