OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    /* This address will always crash should it be actually referenced. */
    RAMRO (rx) : ORIGIN = 0x70000000, LENGTH = 0x00008000
}

SECTIONS
{
    . = ORIGIN(RAMRO);

    .crt0 ALIGN(4) :
    {
        *(.crt0*)
        . = ALIGN(4);
    } >RAMRO

    .text ALIGN(4) :
    {
        *(.text*)
        . = ALIGN(4);
    } >RAMRO

    .rodata ALIGN(4) :
    {
        *(.rodata*)
        . = ALIGN(4);
    } >RAMRO

}
