#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

struct console_fixture {
    unsigned char out[64];
    size_t out_len;
};

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

static int console_putc(void *opaque, unsigned char ch)
{
    struct console_fixture *fixture = opaque;
    if (fixture->out_len >= sizeof(fixture->out)) return -1;
    fixture->out[fixture->out_len++] = ch;
    return 0;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct console_fixture console = {{0}, 0u};
    uint8_t memory[2048] = {0};
    uint32_t steps = 0u;
    uint32_t basepage = 0x100u;
    uint32_t tbase = 0x200u;

    assert(amtari_init(&ctx) == 0);
    assert(strncmp(amtari_version(), "0.2.", 4) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_console_bind(&ctx, console_getc, console_putc, &console) == 0);
    put32(memory, basepage + 0x08u, tbase);

    /* MOVEQ #42,D0 ; NOP ; RTS */
    put16(memory, tbase + 0u, 0x702au);
    put16(memory, tbase + 2u, 0x4e71u);
    put16(memory, tbase + 4u, 0x4e75u);
    assert(amtari_exec_prepare(&ctx, basepage, 0x700u) == 0);
    assert(amtari_exec_run(&ctx, 16u, &steps) == AMTARI_EXEC_HALTED);
    assert(steps == 3u);
    assert(ctx.cpu.d[0] == 42u);

    /* BSR.s subroutine; MOVEQ #7,D1; RTS ; sub: MOVEQ #5,D0; RTS */
    put16(memory, 0x240u, 0x6106u);
    put16(memory, 0x242u, 0x7207u);
    put16(memory, 0x244u, 0x4e75u);
    put16(memory, 0x248u, 0x7005u);
    put16(memory, 0x24au, 0x4e75u);
    ctx.cpu.pc = 0x240u;
    ctx.cpu.a[7] = 0x6fcu;
    put32(memory, 0x6fcu, 0u);
    assert(amtari_exec_run(&ctx, 16u, &steps) == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[0] == 5u);
    assert(ctx.cpu.d[1] == 7u);

    /* Tiny TOS-style program:
       MOVE.L #message,-(SP)
       MOVE.W #9,-(SP)       ; Cconws
       TRAP #1
       ADDQ.L #6,SP
       RTS
    */
    memcpy(&memory[0x300u], "Hello Amtari!", 14u);
    put16(memory, 0x280u, 0x2f3cu);
    put32(memory, 0x282u, 0x300u);
    put16(memory, 0x286u, 0x3f3cu);
    put16(memory, 0x288u, 0x0009u);
    put16(memory, 0x28au, 0x4e41u);
    put16(memory, 0x28cu, 0x5c8fu); /* ADDQ.L #6,A7 */
    put16(memory, 0x28eu, 0x4e75u);
    ctx.cpu.pc = 0x280u;
    ctx.cpu.a[7] = 0x6fcu;
    put32(memory, 0x6fcu, 0u);
    console.out_len = 0u;
    assert(amtari_exec_run(&ctx, 16u, &steps) == AMTARI_EXEC_HALTED);
    assert(console.out_len == 13u);
    assert(memcmp(console.out, "Hello Amtari!", 13u) == 0);
    assert(ctx.cpu.a[7] == 0x700u);

    /* BRA.s skips MOVEQ #1,D2 and lands on MOVEQ #2,D2. */
    put16(memory, 0x340u, 0x6002u);
    put16(memory, 0x342u, 0x7401u);
    put16(memory, 0x344u, 0x7402u);
    ctx.cpu.pc = 0x340u;
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.pc == 0x344u);
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.d[2] == 2u);

    /* GEMDOS Cconin still works through TRAP #1. */
    put16(memory, 0x360u, 0x4e41u);
    put16(memory, 0x500u, 0x0001u);
    ctx.cpu.pc = 0x360u;
    ctx.cpu.a[7] = 0x500u;
    assert(amtari_exec_step(&ctx) == AMTARI_EXEC_RUNNING);
    assert(ctx.cpu.d[0] == (uint32_t)'Q');

    put16(memory, 0x380u, 0xffffu);
    ctx.cpu.pc = 0x380u;
    assert(amtari_exec_step(&ctx) == AMTARI_EILLEGAL);
    return 0;
}
