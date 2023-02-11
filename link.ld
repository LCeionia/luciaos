OUTPUT_FORMAT(binary)
ENTRY(entry)

SECTIONS {
    . = 0x100000;

    .text : {
        *(.text);
    }

    .data : {
        *(.data);
        *(.data*);
        *(.rodata);
        *(.rodata*);
        _edata = .;
    }

    .realmode 0x4000 :
        AT ( _edata )
        { _v86code = .; *(.v86); _ev86code = .; }

    . = _edata + SIZEOF(.realmode);
    .thing : { _loadusercode = .; }

    .usermode 0x400000 : AT(_loadusercode) {
        _usercode = .; *(.user); _eusercode = .;
    }
    .bss 0x400000 (NOLOAD) : AT(_loadusercode + SIZEOF(.usermode)) {
        _bprogstart = .;
        *(.progbss);
        *(.progbss*);
        _bprogend = .;
        *(.proglatebss);
        *(.proglatebss*);
    }

    . = ADDR(.data) + SIZEOF(.data) + SIZEOF(.realmode) + SIZEOF(.usermode);

    .bss : ALIGN(0x1000)
    {
        _bstart = .; *(.bss); *(.bss*) _bend = .;
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
    }
}
