#include "lib/asm_macros.s"

FUNCTION _start, .crt0
    mov     sp, #0x10000000
    bl      otherappMain
    bkpt 1
END_FUNCTION
