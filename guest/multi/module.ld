/* Un módulo de Switch: .text, luego lo de solo lectura, luego lo escribible, cada parte en su página. */
SECTIONS
{
    . = 0;
    .text : { KEEP(*(.text.modstart)) *(.text .text.*) }
    .plt : { *(.plt) }

    . = ALIGN(0x1000);
    .rodata : { *(.rodata .rodata.*) KEEP(*(.rodata.mod0)) }
    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }
    .hash : { *(.hash) }
    .rela.dyn : { *(.rela.dyn) }
    .rela.plt : { *(.rela.plt) }

    . = ALIGN(0x1000);
    .data : { *(.data .data.*) }
    .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) }
    .init_array : { KEEP(*(.init_array .init_array.*)) }
    .dynamic : { *(.dynamic) }
    .got : { *(.got) }
    .got.plt : { *(.got.plt) }

    . = ALIGN(0x1000);
    .bss : { __bss_start = .; *(.bss .bss.* COMMON) __bss_end = .; }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) *(.gnu.version*) }
}
