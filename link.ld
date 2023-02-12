OUTPUT_FORMAT(binary)
ENTRY(entry)

SECTIONS {
    . = 0x100000;
    _USERMODE = 0x400000;

    .text : {
        *(.text);
    }

    .data : {
        *(.data*);
        *(.data);
        *(.rodata);
        *(.rodata*);
        _edata = .;
    }

    .realmode 0x4000 :
        AT ( _edata )
        { _v86code = .; *(.v86); _ev86code = .; }
    . = _edata + SIZEOF(.realmode);

    .bss : ALIGN(0x1000)
    {
        _bstart = .; *(.bss); *(.bss*) _bend = .;
    }

    .bss _USERMODE (NOLOAD) : AT(_bend) {
        _bhexstart = .;
        *(.hexbss);
        *(.hexbss*);
        _bhexend = .;
        *(.hexlatebss);
        *(.hexlatebss*);
    }
    .bss _USERMODE (NOLOAD) : AT(_bend) {
        _btextstart = .;
        *(.textbss);
        *(.textbss*);
        _btextend = .;
        *(.textlatebss);
        *(.textlatebss*);
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
    }
}
