/* Enlaza el programa de prueba a la dirección donde Horizon carga el módulo principal. */
ENTRY(_start)

SECTIONS
{
    . = 0x08000000;
    .text : {
        KEEP(*(.text.crt0))
        *(.text .text.*)
    }

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata .rodata.*)
        KEEP(*(.rodata.mod0))
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data .data.* .data.rel.ro .data.rel.ro.* .got .got.*)
        LONG(0)
    }

    . = ALIGN(0x1000);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
