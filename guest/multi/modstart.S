// Inicio de un módulo NSO: en el offset 4 va el offset de MOD0, que lleva a la sección
// dinámica. Así encuentra rtld (y nxrecomp) las reubicaciones y los símbolos.
    .section .text.modstart, "ax"
    .global __module_start
__module_start:
    .word   0
    .word   __mod0 - __module_start

    .section .rodata.mod0, "a"
    .balign 4
    .hidden __bss_start
    .hidden __bss_end
__mod0:
    .ascii  "MOD0"
    .word   _DYNAMIC - __mod0
    .word   __bss_start - __mod0
    .word   __bss_end - __mod0
    .word   0, 0
    .word   0
