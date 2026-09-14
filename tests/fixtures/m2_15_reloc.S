        .section .text,"ax"
        .globl _start
_start:
        movel global_value,%d0
        rts

        .section .data,"aw"
        .globl global_value
global_value:
        .long 42
