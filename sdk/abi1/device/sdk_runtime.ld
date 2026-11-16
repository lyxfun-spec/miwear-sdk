ENTRY(owned_callback)
SECTIONS {
 . = 0;
 .text : { *(.text.entry) *(.text.signal) *(.text.icon) *(.text.pagesignal) *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }
}
