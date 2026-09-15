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
    assert(strcmp(amtari_version(), AMTARI_VERSION) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    put16(memory, 0x100u, 0x7000u);
    put16(memory, 0x102u, 0x7204u);
    put16(memory, 0x104u, 0x5280u);
    put16(memory, 0x106u, 0x51c9u);
    put16(memory, 0x108u, 0xfffau);
    put16(memory, 0x10au, 0x0c80u);
    put32(memory, 0x10cu, 5u);
    put16(memory, 0x110u, 0x6600u);
    put16(memory, 0x112u, 0x0030u);
    put16(memory, 0x114u, 0x74fdu);
    put16(memory, 0x116u, 0x7607u);
    put16(memory, 0x118u, 0xd483u);
    put16(memory, 0x11au, 0x0c82u);
    put32(memory, 0x11cu, 4u);
    put16(memory, 0x120u, 0x6600u);
    put16(memory, 0x122u, 0x0020u);
    put16(memory, 0x124u, 0x5382u);
    put16(memory, 0x126u, 0x7805u);
    put16(memory, 0x128u, 0x9882u);
    put16(memory, 0x12au, 0x0c84u);
    put32(memory, 0x12cu, 2u);
    put16(memory, 0x130u, 0x6600u);
    put16(memory, 0x132u, 0x0010u);
    put16(memory, 0x134u, 0x7afbu);
    put16(memory, 0x136u, 0x0c85u);
    put32(memory, 0x138u, 0u);
    put16(memory, 0x13cu, 0x6d04u);
    put16(memory, 0x13eu, 0x7cffu);
    put16(memory, 0x140u, 0x4e75u);
    put16(memory, 0x142u, 0x7c2au);
    put16(memory, 0x144u, 0x4e75u);

    reset_handoff(&ctx, memory, 0x100u, 0xefcu);
    assert(amtari_exec_run(&ctx, 128u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[0] == 5u);
    assert((ctx.cpu.d[1] & 0xffffu) == 0xffffu);
    assert(ctx.cpu.d[2] == 3u);
    assert(ctx.cpu.d[4] == 2u);
    assert(ctx.cpu.d[6] == 42u);

    put16(memory, 0x200u, 0x7003u);
    put16(memory, 0x202u, 0x0c80u);
    put32(memory, 0x204u, 0u);
    put16(memory, 0x208u, 0x6e04u);
    put16(memory, 0x20au, 0x7effu);
    put16(memory, 0x20cu, 0x4e75u);
    put16(memory, 0x20eu, 0x7e07u);
    put16(memory, 0x210u, 0x4e75u);
    reset_handoff(&ctx, memory, 0x200u, 0xefcu);
    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[7] == 7u);

    put16(memory, 0x240u, 0x7000u);
    put16(memory, 0x242u, 0x0c80u);
    put32(memory, 0x244u, 0u);
    put16(memory, 0x248u, 0x6c04u);
    put16(memory, 0x24au, 0x72ffu);
    put16(memory, 0x24cu, 0x4e75u);
    put16(memory, 0x24eu, 0x7201u);
    put16(memory, 0x250u, 0x0c81u);
    put32(memory, 0x252u, 1u);
    put16(memory, 0x256u, 0x6f04u);
    put16(memory, 0x258u, 0x72ffu);
    put16(memory, 0x25au, 0x4e75u);
    put16(memory, 0x25cu, 0x7209u);
    put16(memory, 0x25eu, 0x4e75u);
    reset_handoff(&ctx, memory, 0x240u, 0xefcu);
    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[1] == 9u);

    put16(memory, 0x280u, 0x203cu);
    put32(memory, 0x282u, 0x7fffffffu);
    put16(memory, 0x286u, 0x5280u);
    ctx.cpu.pc = 0x280u;
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.d[0] == 0x80000000u);
    assert((ctx.cpu.sr & 0x08u) != 0u);
    assert((ctx.cpu.sr & 0x02u) != 0u);

    return 0;
}
