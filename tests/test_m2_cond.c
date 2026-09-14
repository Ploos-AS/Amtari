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

int main(void)
{
    struct amtari_context ctx = {0};
    uint8_t memory[2048] = {0};
    uint32_t steps = 0u;

    assert(amtari_init(&ctx) == 0);
    assert(strncmp(amtari_version(), "0.2.", 4) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    ctx.cpu.pc = 0x100u;
    ctx.cpu.a[0] = 0x500u;
    ctx.cpu.a[7] = 0x7fcu;
    put32(memory, 0x7fcu, 0u);

    put16(memory, 0x100u, 0x7000u);              /* MOVEQ #0,D0 */
    put16(memory, 0x102u, 0x4a80u);              /* TST.L D0 */
    put16(memory, 0x104u, 0x6600u);              /* BNE.w fail */
    put16(memory, 0x106u, 0x0020u);              /* 0x108 + 0x20 = 0x128 */
    put16(memory, 0x108u, 0x7207u);              /* MOVEQ #7,D1 */
    put16(memory, 0x10au, 0x0c81u);              /* CMPI.L #7,D1 */
    put32(memory, 0x10cu, 7u);
    put16(memory, 0x110u, 0x6600u);              /* BNE.w fail */
    put16(memory, 0x112u, 0x0014u);              /* 0x114 + 0x14 = 0x128 */
    put16(memory, 0x114u, 0x243cu);              /* MOVE.L #imm,D2 */
    put32(memory, 0x116u, 0x12345678u);
    put16(memory, 0x11au, 0x2082u);              /* MOVE.L D2,(A0) */
    put16(memory, 0x11cu, 0x2610u);              /* MOVE.L (A0),D3 */
    put16(memory, 0x11eu, 0x0c83u);              /* CMPI.L #imm,D3 */
    put32(memory, 0x120u, 0x12345678u);
    put16(memory, 0x124u, 0x6700u);              /* BEQ.w success */
    put16(memory, 0x126u, 0x0004u);              /* 0x128 + 4 = 0x12c */
    put16(memory, 0x128u, 0x78ffu);              /* fail: MOVEQ #-1,D4 */
    put16(memory, 0x12au, 0x4e75u);              /* RTS */
    put16(memory, 0x12cu, 0x782au);              /* success: MOVEQ #42,D4 */
    put16(memory, 0x12eu, 0x4e75u);              /* RTS */

    assert(amtari_exec_run(&ctx, 64u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[4] == 42u);
    assert(ctx.cpu.d[3] == 0x12345678u);
    assert(memory[0x500u] == 0x12u && memory[0x501u] == 0x34u &&
           memory[0x502u] == 0x56u && memory[0x503u] == 0x78u);

    return 0;
}
