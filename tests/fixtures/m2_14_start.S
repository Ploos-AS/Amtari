        .section .text.startup,"ax"
        .globl _start
        .extern amtari_entry
_start:
        jsr amtari_entry
        movew %d0,%sp@-
        movew #0x004c,%sp@-
        trap #1
        rts
