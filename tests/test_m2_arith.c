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
    assert(strcmp(amtari_version(), "0.2.8-m2") == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    /*
       D0 = 0; D1 = 4
       loop: ADDQ.L #1,D0
             DBRA D1,loop
       Verify D0 == 5, then exercise ADD.L, SUBQ.L, SUB.L and BLT.
    */
    put16(memory, 0x100u, 0x7000u);              /* MOVEQ #0,D0 */
    put16(memory, 0x102u, 0x7204u);              /* MOVEQ #4,D1 */
    put16(memory, 0x104u, 0x5280u);              /* ADDQ.L #1,D0 */
    put16(memory, 0x106u, 0x51c9u);              /* DBRA D1,loop */
    put16(memory, 0x108u, 0xfffau);              /* 0x104 - 0x10a */
    put16(memory, 0x10au, 0x0c80u);              /* CMPI.L #5,D0 */
    put32(memory, 0x10cu, 5u);
    put16(memory, 0x110u, 0x6600u);              /* BNE fail */
    put16(memory, 0x112u, 0x0030u);              /* -> 0x144 */
    put16(memory, 0x114u, 0x74fdu);              /* MOVEQ #-3,D2 */
    put16(memory, 0x116u, 0x7607u);              /* MOVEQ #7,D3 */
    put16(memory, 0x118u, 0xd483u);              /* ADD.L D3,D2 => 4 */
    put16(memory, 0x11au, 0x0c82u);              /* CMPI.L #4,D2 */
    put32(memory, 0x11cu, 4u);
    put16(memory, 0x120u, 0x6600u);              /* BNE fail */
    put16(memory, 0x122u, 0x0020u);              /* -> 0x144 */
    put16(memory, 0x124u, 0x5382u);              /* SUBQ.L #1,D2 => 3 */
    put16(memory, 0x126u, 0x7805u);              /* MOVEQ #5,D4 */
    put16(memory, 0x128u, 0x9882u);              /* SUB.L D2,D4 => 2 */
    put16(memory, 0x12au, 0x0c84u);              /* CMPI.L #2,D4 */
    put32(memory, 0x12cu, 2u);
    put16(memory, 0x130u, 0x6600u);              /* BNE fail */
    put16(memory, 0x132u, 0x0010u);              /* -> 0x144 */
    put16(memory, 0x134u, 0x7afbu);              /* MOVEQ #-5,D5 */
    put16(memory, 0x136u, 0x0c85u);              /* CMPI.L #0,D5 */
    put32(memory, 0x138u, 0u);
    put16(memory, 0x13cu, 0x6d04u);              /* BLT.s success -> 0x142 */
    put16(memory, 0x13eu, 0x7cffu);              /* fail-local: MOVEQ #-1,D6 */
    put16(memory, 0x140u, 0x4e75u);              /* RTS */
    put16(memory, 0x142u, 0x7c2au);              /* success: MOVEQ #42,D6 */
    put16(memory, 0x144u, 0x4e75u);              /* fail target / success RTS */

    reset_handoff(&ctx, memory, 0x100u, 0xefcu);
    assert(amtari_exec_run(&ctx, 128u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[0] == 5u);
    assert((ctx.cpu.d[1] & 0xffffu) == 0xffffu);
    assert(ctx.cpu.d[2] == 3u);
    assert(ctx.cpu.d[4] == 2u);
    assert(ctx.cpu.d[6] == 42u);

    /* BGT on a positive value. */
    put16(memory, 0x200u, 0x7003u);              /* MOVEQ #3,D0 */
    put16(memory, 0x202u, 0x0c80u);              /* CMPI.L #0,D0 */
    put32(memory, 0x204u, 0u);
    put16(memory, 0x208u, 0x6e04u);              /* BGT.s -> 0x20e */
    put16(memory, 0x20au, 0x7effu);              /* MOVEQ #-1,D7 */
    put16(memory, 0x20cu, 0x4e75u);
    put16(memory, 0x20eu, 0x7e07u);              /* MOVEQ #7,D7 */
    put16(memory, 0x210u, 0x4e75u);
    reset_handoff(&ctx, memory, 0x200u, 0xefcu);
    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[7] == 7u);

    /* BGE and BLE both accept equality. */
    put16(memory, 0x240u, 0x7000u);
    put16(memory, 0x242u, 0x0c80u);
    put32(memory, 0x244u, 0u);
    put16(memory, 0x248u, 0x6c04u);              /* BGE.s -> 0x24e */
    put16(memory, 0x24au, 0x72ffu);
    put16(memory, 0x24cu, 0x4e75u);
    put16(memory, 0x24eu, 0x7201u);
    put16(memory, 0x250u, 0x0c81u);              /* CMPI.L #1,D1 */
    put32(memory, 0x252u, 1u);
    put16(memory, 0x256u, 0x6f04u);              /* BLE.s -> 0x25c */
    put16(memory, 0x258u, 0x72ffu);
    put16(memory, 0x25au, 0x4e75u);
    put16(memory, 0x25cu, 0x7209u);              /* success */
    put16(memory, 0x25eu, 0x4e75u);
    reset_handoff(&ctx, memory, 0x240u, 0xefcu);
    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[1] == 9u);

    /* Signed overflow: 0x7fffffff + 1 => N=1, V=1. */
    put16(memory, 0x280u, 0x203cu);
    put32(memory, 0x282u, 0x7fffffffu);
    put16(memory, 0x286u, 0x5280u);              /* ADDQ.L #1,D0 */
    ctx.cpu.pc = 0x280u;
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.d[0] == 0x80000000u);
    assert((ctx.cpu.sr & 0x08u) != 0u);           /* N */
    assert((ctx.cpu.sr & 0x02u) != 0u);           /* V */

    return 0;
}
