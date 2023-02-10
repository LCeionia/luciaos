OUTPUT_FORMAT(binary)
ENTRY(entry)

SECTIONS {
    . = 0x100000;

    .text : {
        *(.text);
    }

    .data : {
        *(.data);
        *(.rodata);
        *(.rodata*);
        _edata = .;
    }

    .realmode 0x4000 :
        AT ( ADDR(.data) + SIZEOF(.data) )
        { _v86code = .; *(.v86); _ev86code = .; }

    . = ADDR(.data) + SIZEOF(.data) + SIZEOF(.realmode);
    .thing : { _loadusercode = .; }

    .usermode 0x400000 :
        AT ( ADDR(.data) + SIZEOF(.data) + SIZEOF(.realmode) )
        { _usercode = .; *(.user); _eusercode = .; }

    .bss 0x400000 : {
        _bprogstart = .;
        hexedit.o(.bss);
        _bprogend = .;
        hexedit.o(.bss.end);
    }

    . = ADDR(.data) + SIZEOF(.data) + SIZEOF(.realmode) + SIZEOF(.usermode);

    .bss : ALIGN(0x1000)
    {
        _bstart = .; *(.bss); _bend = .;
    }
}
