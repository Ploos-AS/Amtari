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

static int console_getc(void *opaque)
{
    (void)opaque;
    return 'Q';
}

int main(void)
{
    struct amtari_context ctx = {0};
    uint8_t memory[1024] = {0};
    uint32_t steps = 0u;
    uint32_t basepage = 0x100u;
    uint32_t tbase = 0x200u;

    assert(amtari_init(&ctx) == 0);
    assert(strncmp(amtari_version(), "0.2.", 4) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_console_bind(&ctx, console_getc, 0, 0) == 0);

    /* Minimal basepage handoff: p_tbase at offset 0x08. */
    put32(memory, basepage + 0x08u, tbase);

    /* MOVEQ #42,D0 ; NOP ; RTS */
    put16(memory, tbase + 0u, 0x702au);
    put16(memory, tbase + 2u, 0x4e71u);
    put16(memory, tbase + 4u, 0x4e75u);

    assert(amtari_exec_prepare(&ctx, basepage, 0x400u) == 0);
    assert(ctx.current_basepage == basepage);
    assert(ctx.cpu.a[0] == basepage);
    assert(ctx.cpu.a[7] == 0x3fcu);
    assert(ctx.cpu.pc == tbase);

    assert(amtari_exec_run(&ctx, 16u, &steps) == AMTARI_EXEC_HALTED);
    assert(steps == 3u);
    assert(ctx.cpu.d[0] == 42u);
    assert(ctx.cpu.a[7] == 0x400u);

    /* GEMDOS TRAP #1 reads its function word at SP and returns in D0. */
    put16(memory, 0x220u, 0x4e41u);
    put16(memory, 0x300u, 0x0001u); /* Cconin */
    ctx.cpu.pc = 0x220u;
    ctx.cpu.a[7] = 0x300u;
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.pc == 0x222u);
    assert(ctx.cpu.a[7] == 0x300u);
    assert(ctx.cpu.d[0] == (uint32_t)'Q');

    /* Unsupported opcode fails explicitly instead of being guessed. */
    put16(memory, 0x240u, 0xffffu);
    ctx.cpu.pc = 0x240u;
    assert(amtari_exec_step(&ctx) == AMTARI_EILLEGAL);

    return 0;
}
