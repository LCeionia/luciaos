#pragma once
#include <stdint.h>

struct interrupt_frame {
    uint32_t eip, cs;
    uint32_t eflags;
    uint32_t esp, ss;
    uint32_t es, ds, fs, gs;
};
__attribute__ ((interrupt))
void gpf_handler_v86(struct interrupt_frame *frame, unsigned long error_code);

void setup_interrupts();
