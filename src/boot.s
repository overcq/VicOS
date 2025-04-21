; Multiboot header
section .multiboot
align 4
    dd 0x1BADB002           ; magic
    dd 0x00                 ; flags
    dd -(0x1BADB002 + 0x00) ; checksum

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Call kernel_main
    call kernel_main

    ; Hang if kernel_main returns
.hang:
    cli      ; Disable interrupts
    hlt      ; Halt the CPU
    jmp .hang ; Loop if we somehow wake up

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:
