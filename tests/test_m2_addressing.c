#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

static void put16(uint8_t *memory, uint32_t address, uint16_t value)
{
    memory[address] = (uint8_t)(value >> 8);
    memory[address + 1u] = (uint8_t)value;
}

static void put32(uint8_t *memory, uint32_t address, uint32_t value)
{
    memory[address] = (uint8_t)(value >> 24);
    memory[address + 1u] = (uint8_t)(value >> 16);
    memory[address + 2u] = (uint8_t)(value >> 8);
    memory[address + 3u] = (uint8_t)value;
}

static uint32_t get32(const uint8_t *memory, uint32_t address)
{
    return ((uint32_t)memory[address] << 24) |
           ((uint32_t)memory[address + 1u] << 16) |
           ((uint32_t)memory[address + 2u] << 8) |
           (uint32_t)memory[address + 3u];
}

int main(void)
{
    struct amtari_context ctx = {0};
    uint8_t memory[4096] = {0};
    uint32_t steps = 0u;

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), AMTARI_VERSION) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    ctx.cpu.pc = 0x100u;
    ctx.cpu.a[6] = 0x66666666u;
    ctx.cpu.a[7] = 0xefcu;
    put32(memory, 0xefcu, 0u); /* host handoff sentinel */

    /* Compiler-shaped frame and local-variable data flow:
       link a6,#-8
       move.l #$11223344,d0
       move.l d0,-4(a6)
       move.l -4(a6),d1
       lea -4(a6),a0
       move.l (a0)+,d2
       movea.l #$500,a2
       move.l d2,(a2)+
       move.l -(a2),d3
       unlk a6
       rts
    */
    put16(memory, 0x100u, 0x4e56u); /* LINK A6,#-8 */
    put16(memory, 0x102u, 0xfff8u);
    put16(memory, 0x104u, 0x203cu); /* MOVE.L #imm,D0 */
    put32(memory, 0x106u, 0x11223344u);
    put16(memory, 0x10au, 0x2d40u); /* MOVE.L D0,-4(A6) */
    put16(memory, 0x10cu, 0xfffcu);
    put16(memory, 0x10eu, 0x222eu); /* MOVE.L -4(A6),D1 */
    put16(memory, 0x110u, 0xfffcu);
    put16(memory, 0x112u, 0x41eeu); /* LEA -4(A6),A0 */
    put16(memory, 0x114u, 0xfffcu);
    put16(memory, 0x116u, 0x2418u); /* MOVE.L (A0)+,D2 */
    put16(memory, 0x118u, 0x247cu); /* MOVEA.L #$500,A2 */
    put32(memory, 0x11au, 0x00000500u);
    put16(memory, 0x11eu, 0x24c2u); /* MOVE.L D2,(A2)+ */
    put16(memory, 0x120u, 0x2622u); /* MOVE.L -(A2),D3 */
    put16(memory, 0x122u, 0x4e5eu); /* UNLK A6 */
    put16(memory, 0x124u, 0x4e75u); /* RTS */

    assert(amtari_exec_run(&ctx, 64u, &steps) == AMTARI_EXEC_HALTED);
    assert(steps == 11u);
    assert(ctx.cpu.d[0] == 0x11223344u);
    assert(ctx.cpu.d[1] == 0x11223344u);
    assert(ctx.cpu.d[2] == 0x11223344u);
    assert(ctx.cpu.d[3] == 0x11223344u);
    assert(ctx.cpu.a[0] == 0x0ef8u); /* LEA local then postincrement */
    assert(ctx.cpu.a[2] == 0x0500u); /* postincrement then predecrement */
    assert(ctx.cpu.a[6] == 0x66666666u);
    assert(ctx.cpu.a[7] == 0x0f00u);
    assert(get32(memory, 0x0ef4u) == 0x11223344u);
    assert(get32(memory, 0x0500u) == 0x11223344u);

    /* MOVEA.L d16(An),Am: load a pointer without changing CCR. */
    put32(memory, 0x0604u, 0x00000700u);
    ctx.cpu.a[1] = 0x0600u;
    ctx.cpu.sr = 0x0004u;
    ctx.cpu.pc = 0x180u;
    put16(memory, 0x180u, 0x2669u); /* MOVEA.L 4(A1),A3 */
    put16(memory, 0x182u, 0x0004u);
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.a[3] == 0x0700u);
    assert((ctx.cpu.sr & 0x001fu) == 0x0004u);

    return 0;
}
