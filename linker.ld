ENTRY(_start)

SECTIONS {
    /* Start at 1MB */
    . = 0x100000;

    /* Multiboot header first */
    .text : {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
    }

    /* Initialized data */
    .data : {
        *(.data)
    }

    /* Uninitialized data */
    .bss : {
        *(.bss)
        *(COMMON)
    }
}
