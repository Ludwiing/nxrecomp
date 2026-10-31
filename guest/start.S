// Arranque de un NRO mínimo, igual que el crt0 de un homebrew:
// los primeros 0x80 bytes del .text son la cabecera NRO que llena tools/mknro.py.
    .section .text.crt0, "ax"
    .global _start
_start:
    b       real_start
    .word   __mod0 - _start         // offset de MOD0, lo lee el cargador
    .fill   0x78, 1, 0              // espacio para la cabecera NRO0 (0x10..0x80)

real_start:
    mov     x29, #0
    bl      nx_main
    svc     #0x7                    // svcExitProcess: no vuelve
    b       .

    .section .rodata.mod0, "a"
    .balign 4
    .global __mod0
__mod0:
    .ascii  "MOD0"
    .word   0                       // sin .dynamic: enlazado a dirección fija
    .word   __bss_start - __mod0
    .word   __bss_end - __mod0
    .word   0, 0                    // eh_frame_hdr
    .word   0                       // objeto del módulo
