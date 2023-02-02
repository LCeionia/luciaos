#include <stdint.h>

struct interrupt_frame {
    uint32_t eip, cs;
    uint32_t eflags;
    uint32_t esp, ss;
    uint32_t es, ds, fs, gs;
};

/* Real Mode helper macros */
/* segment:offset pair */
typedef uint32_t FARPTR;

/* Make a FARPTR from a segment and an offset */
#define MK_FP(seg, off)  ((FARPTR) (((uint32_t) (seg) << 16) | (uint16_t) (off)))

/* Extract the segment part of a FARPTR */
#define FP_SEG(fp)        (((FARPTR) fp) >> 16)

/* Extract the offset part of a FARPTR */
#define FP_OFF(fp)        (((FARPTR) fp) & 0xffff)

/* Convert a segment:offset pair to a linear address */
#define FP_TO_LINEAR(seg, off) ((void*)(uintptr_t)((((uint32_t)seg) << 4) + ((uint32_t)off)))

#define EFLAG_IF ((uint32_t)1 << 9)
#define EFLAG_VM ((uint32_t)1 << 17)

FARPTR i386LinearToFp(void *ptr);

__attribute((__no_caller_saved_registers__))
void kbd_wait();

__attribute((__no_caller_saved_registers__))
uint8_t get_key();

__attribute__ ((interrupt))
void gpf_handler_v86(struct interrupt_frame *frame, unsigned long error_code);

void setup_interrupts();
