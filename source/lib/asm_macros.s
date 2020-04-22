.macro FUNCTION name, section=.text\.name
    .section        \section, "ax", %progbits
    .align          3
    .global         \name
    .type           \name, %function
    .func           \name
    .cfi_sections   .debug_frame
    .cfi_startproc
    \name:
.endm

.macro END_FUNCTION
    .cfi_endproc
    .endfunc
.endm
