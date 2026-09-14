#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

static const uint8_t child_prg[] = {
    0x60,0x1a, 0x00,0x00,0x00,0x0c, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x01,
    /* moveq #42,d0 ; move.w d0,-(sp) ; move.w #$4c,-(sp) ; trap #1 ; rts */
    0x70,0x2a, 0x3f,0x00, 0x3f,0x3c,0x00,0x4c, 0x4e,0x41, 0x4e,0x75
};

static int fetch_program(void *opaque, const char *path, const uint8_t **data, size_t *size)
{
    int *fetch_count = (int *)opaque;
    ++*fetch_count;
    assert(strstr(path, "CHILD.PRG") != 0);
    *data = child_prg;
    *size = sizeof(child_prg);
    return 0;
}

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
    struct amtari_cpu_state parent_cpu;
    uint8_t memory[32768] = {0};
    int fetch_count = 0;
    int32_t rc;

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), "0.2.16-m2") == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_program_bind(&ctx, fetch_program, &fetch_count) == 0);

    memcpy(&memory[0x0100u], "CHILD.PRG", 10u);
    ctx.current_basepage = 0x0800u;
    ctx.cpu.d[1] = 0x11223344u;
    ctx.cpu.a[2] = 0x55667788u;
    ctx.cpu.pc = 0x2222u;
    ctx.cpu.a[7] = 0x0400u;
    ctx.cpu.sr = 0x0010u;
    parent_cpu = ctx.cpu;

    /* GEMDOS Pexec(0,name,cmdline,env). */
    put16(memory, 0x0400u, 0x004bu);
    put16(memory, 0x0402u, 0x0000u);
    put32(memory, 0x0404u, 0x0100u);
    put32(memory, 0x0408u, 0x0000u);
    put32(memory, 0x040cu, 0x0000u);

    rc = amtari_gemdos_dispatch(&ctx, 0x4bu);
    assert(rc == 42);
    assert(fetch_count == 1);
    assert(ctx.process_depth == 0u);
    assert(ctx.current_basepage == 0x0800u);
    assert(ctx.next_load_address == 0x1000u);
    assert(memcmp(&ctx.cpu, &parent_cpu, sizeof(parent_cpu)) == 0);

    /* Through the trap core, the returned child exit code becomes parent D0. */
    rc = amtari_trap_dispatch(&ctx, 1u, 0x4bu);
    assert(rc == 42);
    assert(fetch_count == 2);

    /* First implementation deliberately refuses nested child execution. */
    ctx.process_depth = 1u;
    assert(amtari_gemdos_dispatch(&ctx, 0x4bu) == AMTARI_ENOSYS);

    return 0;
}
