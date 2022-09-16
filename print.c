#include "print.h"

char nibbleToHex(uint8_t n) {
    return n > 9 ? (n - 10) + 'A' : n + '0';
}
void printByte(uint8_t v, uint16_t *buff) {
    *(char *)&buff[0] = nibbleToHex((v >> 4) & 0xF);
    *(char *)&buff[1] = nibbleToHex(v & 0xF);
}
void printWord(uint16_t v, uint16_t *buff) {
    printByte(v >> 8, buff);
    printByte(v, &buff[2]);
}
void printDword(uint32_t v, uint16_t *buff) {
    printWord(v >> 16, buff);
    printWord(v, &buff[4]);
}
