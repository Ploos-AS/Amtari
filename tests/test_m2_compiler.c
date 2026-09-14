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

static void reset_handoff(struct amtari_context *ctx, uint8_t *memory,
                          uint32_t pc, uint32_t sp)
{
    ctx->cpu.pc = pc;
    ctx->cpu.a[7] = sp;
    put32(memory, sp, 0u);
}

int main(void)
{
    struct amtari_context ctx = {0};
    uint8_t memory[4096] = {0};
    uint32_t steps = 0u;

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), "0.2.10-m2") == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    /* Compiler-like frame using byte/word loads, EXT, logical ops and CMP. */
    ctx.cpu.a[6] = 0x77777777u;
    put16(memory, 0x100u, 0x4e56u);              /* LINK A6,#-8 */
    put16(memory, 0x102u, 0xfff8u);
    put16(memory, 0x104u, 0x203cu);              /* MOVE.L #0x11223380,D0 */
    put32(memory, 0x106u, 0x11223380u);
    put16(memory, 0x10au, 0x2d40u);              /* MOVE.L D0,-4(A6) */
    put16(memory, 0x10cu, 0xfffcu);
    put16(memory, 0x10eu, 0x122eu);              /* MOVE.B -1(A6),D1 */
    put16(memory, 0x110u, 0xffffu);
    put16(memory, 0x112u, 0x4881u);              /* EXT.W D1 */
    put16(memory, 0x114u, 0x48c1u);              /* EXT.L D1 */
    put16(memory, 0x116u, 0x342eu);              /* MOVE.W -4(A6),D2 */
    put16(memory, 0x118u, 0xfffcu);
    put16(memory, 0x11au, 0x4283u);              /* CLR.L D3 */
    put16(memory, 0x11cu, 0x3602u);              /* MOVE.W D2,D3 */
    put16(memory, 0x11eu, 0x283cu);              /* MOVE.L #0x00ff00ff,D4 */
    put32(memory, 0x120u, 0x00ff00ffu);
    put16(memory, 0x124u, 0xc084u);              /* AND.L D4,D0 */
    put16(memory, 0x126u, 0x2a3cu);              /* MOVE.L #0xff000000,D5 */
    put32(memory, 0x128u, 0xff000000u);
    put16(memory, 0x12cu, 0x8085u);              /* OR.L D5,D0 */
    put16(memory, 0x12eu, 0x2c3cu);              /* MOVE.L #0x00220080,D6 */
    put32(memory, 0x130u, 0x00220080u);
    put16(memory, 0x134u, 0xbd80u);              /* EOR.L D6,D0 */
    put16(memory, 0x136u, 0xb085u);              /* CMP.L D5,D0 */
    put16(memory, 0x138u, 0x6606u);              /* BNE.s fail */
    put16(memory, 0x13au, 0x4e5eu);              /* UNLK A6 */
    put16(memory, 0x13cu, 0x7e2au);              /* MOVEQ #42,D7 */
    put16(memory, 0x13eu, 0x4e75u);              /* RTS */
    put16(memory, 0x140u, 0x7effu);              /* fail: MOVEQ #-1,D7 */
    put16(memory, 0x142u, 0x4e5eu);              /* UNLK A6 */
    put16(memory, 0x144u, 0x4e75u);              /* RTS */

    reset_handoff(&ctx, memory, 0x100u, 0xefcu);
    assert(amtari_exec_run(&ctx, 128u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[1] == 0xffffff80u);
    assert((ctx.cpu.d[2] & 0xffffu) == 0x1122u);
    assert(ctx.cpu.d[3] == 0x00001122u);
    assert(ctx.cpu.d[0] == 0xff000000u);
    assert(ctx.cpu.d[7] == 42u);
    assert(ctx.cpu.a[6] == 0x77777777u);
    assert(ctx.cpu.a[7] == 0xf00u);

    /* Byte/word postincrement and predecrement addressing. */
    ctx.cpu.a[0] = 0x500u;
    put16(memory, 0x200u, 0x10fcu);              /* MOVE.B #0x7f,(A0)+ */
    put16(memory, 0x202u, 0x007fu);
    put16(memory, 0x204u, 0x30fcu);              /* MOVE.W #0x1234,(A0)+ */
    put16(memory, 0x206u, 0x1234u);
    put16(memory, 0x208u, 0x3020u);              /* MOVE.W -(A0),D0 */
    put16(memory, 0x20au, 0x1220u);              /* MOVE.B -(A0),D1 */
    put16(memory, 0x20cu, 0x4e75u);

    reset_handoff(&ctx, memory, 0x200u, 0xefcu);
    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.a[0] == 0x500u);
    assert((ctx.cpu.d[0] & 0xffffu) == 0x1234u);
    assert((ctx.cpu.d[1] & 0xffu) == 0x7fu);
    assert(memory[0x500u] == 0x7fu);
    assert(memory[0x501u] == 0x12u && memory[0x502u] == 0x34u);

    return 0;
}
