        .section .text,"ax"
        .globl _start
_start:
        .word 0x2039
        .long global_value
        rts

        .section .data,"aw"
        .globl global_value
global_value:
        .long 42
