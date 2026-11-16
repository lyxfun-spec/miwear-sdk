ENTRY(pg_init)
SECTIONS{. = 0;.text : {*(.text*) *(.rodata*)} .data : {*(.data*)} .bss : {*(.bss*) *(COMMON)} /DISCARD/ : {*(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*)}}
