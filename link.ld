OUTPUT_FORMAT(binary)
ENTRY(entry)

SECTIONS {
    . = 0x8000;

    .text : ALIGN(0x1000) {
        *(.text)
    }

    .data : ALIGN(0x1000) {
        *(.data)
        *(.rodata)
    }

    .bss : ALIGN(0x1000) {
        *(.bss)
    }
}
