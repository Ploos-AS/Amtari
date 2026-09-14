/* M2.13 deterministic 68000 brief indexed-addressing fixture.
 *
 * 0x1030 0x0800 = MOVE.B 0(A0,D0.L),D0 on a 68000 brief extension.
 */
        .text
        .globl amtari_entry
amtari_entry:
        .word 0x7005              /* MOVEQ #5,D0 */
        .word 0x41fa,0x000c       /* LEA table(PC),A0 */
        .word 0x1030,0x0800       /* MOVE.B 0(A0,D0.L),D0 */
        .word 0x4880              /* EXT.W D0 */
        .word 0x48c0              /* EXT.L D0 */
        .word 0x4e75              /* RTS */
table:
        .byte 0,0,0,0,0,42
