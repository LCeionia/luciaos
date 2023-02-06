#include "interrupt.h"
#include "kbd.h"

uint8_t scancodesToAscii[0x3B] =
"\0\0" // 0x00 - 0x01
"1234567890" // 0x02 - 0x0B
"-=" // 0x0C - 0x0D
"\0\0" // 0x0E - 0x0F
"qwertyuiop[]" // 0x10 - 0x1B
"\0\0" // 0x1C - 0x1D
"asdfghjkl;'`" // 0x1E - 0x29
"\0" // 0x2A
"\\zxcvbnm,./" // 0x2B - 0x35
"\0" // 0x36
"*" // 0x37
"\0" // 0x38
" " // 0x39
"\0"; // 0x3A
uint8_t scancodesToAsciiShift[0x3B] =
"\0\0" // 0x00 - 0x01
"!@#$%^&*()" // 0x02 - 0x0B
"_+" // 0x0C - 0x0D
"\0\0" // 0x0E - 0x0F
"QWERTYUIOP{}" // 0x10 - 0x1B
"\0\0" // 0x1C - 0x1D
"ASDFGHJKL:\"~" // 0x1E - 0x29
"\0" // 0x2A
"|ZXCVBNM<>?" // 0x2B - 0x35
"\0" // 0x36
"*" // 0x37
"\0" // 0x38
" " // 0x39
"\0"; // 0x3A
uint8_t _KBDWAIT;
uint8_t _KEYCAPS = 0, _KEYSHIFT = 0;
uint8_t _LSTKEY_ASCII = 0, _LSTKEY_SCAN = 0;
__attribute__ ((interrupt))
void keyboardHandler(struct interrupt_frame *frame) {
    uint16_t old_ds;
    asm volatile(
        "mov %%ds, %%bx\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        :"=b"(old_ds)::"%ax"
    );
    uint8_t key;
    asm volatile("inb $0x60, %%al":"=a"(key));
    if (key == 0x3A) { // caps lock press
        _KEYCAPS = !_KEYCAPS;
    } else if (key == 0x2A || key == 0x36) { // left and right shift press
        _KEYSHIFT = 1;
    } else if (key == 0xAA || key == 0xB6) { // left and right shift release
        _KEYSHIFT = 0;
    } else if (key < 0x3B) {
        uint8_t ascii = _KEYCAPS != _KEYSHIFT ?
            scancodesToAsciiShift[key] :
            scancodesToAscii[key];
        if (ascii) {
            _LSTKEY_ASCII = ascii;
            _LSTKEY_SCAN = key;
            _KBDWAIT = 1;
        }
    } else {
        _LSTKEY_ASCII = 0;
        _LSTKEY_SCAN = key;
    }
    asm volatile("outb %%al, $0x20"::"a"(0x20));
    asm volatile(
        "mov %%ax, %%ds\n"
        ::"a"(old_ds)
    );
}

__attribute((__no_caller_saved_registers__))
void kbd_wait() {
    _KBDWAIT = 0;
    while(!_KBDWAIT) {
        asm volatile("hlt");
    }
}

__attribute((__no_caller_saved_registers__))
uint8_t get_key() {
    while(!_LSTKEY_ASCII) {
        asm volatile("hlt");
    }
    uint8_t k = _LSTKEY_ASCII;
    _LSTKEY_ASCII = 0;
    return k;
}
__attribute((__no_caller_saved_registers__))
uint16_t get_scancode() {
    while(!_LSTKEY_SCAN) {
        asm volatile("hlt");
    }
    uint16_t k = _LSTKEY_SCAN | (_LSTKEY_ASCII << 8);
    _LSTKEY_SCAN = 0;
    _LSTKEY_ASCII = 0;
    return k;
}

