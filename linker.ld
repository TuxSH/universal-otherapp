OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    RAMRO (rx) : ORIGIN = 0x00101000, LENGTH = 0x0000C000
    RAMRW (rw!i) : ORIGIN = 0x8000000, LENGTH = 0x00100000
}

SECTIONS
{
    .text : ALIGN(4)
    {
        *(.crt0*)
        *(.text*)
        . = ALIGN(4);
    } >RAMRO

    .rodata ALIGN(4) :
    {
        *(.rodata*)
        . = ALIGN(4);
    } >RAMRO
    
}
