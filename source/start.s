#include "lib/asm_macros.s.h"

FUNCTION _start, .crt0
    // Assume sp has been correctly set, we don't use the stack really much
    //mov     sp, #0x10000000
    adr     r1, kernelhaxcode_3ds_bin_size
    ldr     r2, [r1], #4
    bl      otherappMain
    bkpt    1
END_FUNCTION

.global     kernelhaxcode_3ds_bin_size
kernelhaxcode_3ds_bin_size:
    .word   _kernelhaxcode_3ds_bin_end - kernelhaxcode_3ds_bin

.global     kernelhaxcode_3ds_bin
kernelhaxcode_3ds_bin:
    .incbin "../kernelhaxcode_3ds/kernelhaxcode_3ds.bin"

.hidden     _kernelhaxcode_3ds_bin_end
_kernelhaxcode_3ds_bin_end:
