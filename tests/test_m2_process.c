#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

static const uint8_t grand_prg[] = {
    0x60,0x1a, 0x00,0x00,0x00,0x0c, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x01,
    0x70,0x28, 0x3f,0x00, 0x3f,0x3c,0x00,0x4c, 0x4e,0x41, 0x4e,0x75
};

static const uint8_t child_prg[] = {
    0x60,0x1a, 0x00,0x00,0x00,0x2c, 0x00,0x00,0x00,0x0a,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x01,
    0x2f,0x3c,0x00,0x00,0x00,0x00,
    0x2f,0x3c,0x00,0x00,0x00,0x00,
    0x2f,0x3c,0x00,0x00,0x11,0x2c,
    0x3f,0x3c,0x00,0x00,
    0x3f,0x3c,0x00,0x4b,
    0x4e,0x41,
    0x4f,0xef,0x00,0x10,
    0x54,0x80,
    0x3f,0x00,
    0x3f,0x3c,0x00,0x4c,
    0x4e,0x41,
    0x4e,0x75,
    'G','R','A','N','D','.','P','R','G',0x00
};

struct fetch_fixture { int child_count; int grand_count; };

static int fetch_program(void *opaque, const char *path, const uint8_t **data, size_t *size)
{
    struct fetch_fixture *fixture = (struct fetch_fixture *)opaque;
    if (strstr(path, "CHILD.PRG") != 0) {
        ++fixture->child_count; *data = child_prg; *size = sizeof(child_prg); return 0;
    }
    if (strstr(path, "GRAND.PRG") != 0) {
        ++fixture->grand_count; *data = grand_prg; *size = sizeof(grand_prg); return 0;
    }
    return AMTARI_ENOENT;
}

static void put16(uint8_t *memory, uint32_t address, uint16_t value)
{
    memory[address] = (uint8_t)(value >> 8); memory[address + 1u] = (uint8_t)value;
}

static void put32(uint8_t *memory, uint32_t address, uint32_t value)
{
    memory[address] = (uint8_t)(value >> 24); memory[address + 1u] = (uint8_t)(value >> 16);
    memory[address + 2u] = (uint8_t)(value >> 8); memory[address + 3u] = (uint8_t)value;
}

static uint32_t get32(const uint8_t *memory, uint32_t address)
{
    return ((uint32_t)memory[address] << 24) | ((uint32_t)memory[address + 1u] << 16) |
           ((uint32_t)memory[address + 2u] << 8) | (uint32_t)memory[address + 3u];
}

static void setup_parent_pexec(uint8_t *memory, struct amtari_context *ctx)
{
    ctx->cpu.a[7] = 0x0400u;
    put16(memory, 0x0400u, 0x004bu); put16(memory, 0x0402u, 0x0000u);
    put32(memory, 0x0404u, 0x0100u); put32(memory, 0x0408u, 0x0000u);
    put32(memory, 0x040cu, 0x0000u);
}

static int32_t call_malloc(uint8_t *memory, struct amtari_context *ctx, uint32_t amount)
{
    ctx->cpu.a[7] = 0x0500u;
    put16(memory, 0x0500u, 0x0048u);
    put32(memory, 0x0502u, amount);
    return amtari_gemdos_dispatch(ctx, 0x48u);
}

static int32_t call_mfree(uint8_t *memory, struct amtari_context *ctx, uint32_t address)
{
    ctx->cpu.a[7] = 0x0500u;
    put16(memory, 0x0500u, 0x0049u);
    put32(memory, 0x0502u, address);
    return amtari_gemdos_dispatch(ctx, 0x49u);
}

static int32_t call_mshrink(uint8_t *memory, struct amtari_context *ctx,
                            uint32_t address, uint32_t size)
{
    ctx->cpu.a[7] = 0x0500u;
    put16(memory, 0x0500u, 0x004au);
    put16(memory, 0x0502u, 0u);
    put32(memory, 0x0504u, address);
    put32(memory, 0x0508u, size);
    return amtari_gemdos_dispatch(ctx, 0x4au);
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct amtari_cpu_state parent_cpu;
    struct fetch_fixture fixture = {0};
    uint8_t memory[32768] = {0};
    int32_t rc;
    int32_t a;
    int32_t b;
    int32_t c;

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), "0.2.19-m2") == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_program_bind(&ctx, fetch_program, &fixture) == 0);

    memcpy(&memory[0x0100u], "CHILD.PRG", 10u);
    ctx.current_basepage = 0x0800u;
    ctx.cpu.d[1] = 0x11223344u; ctx.cpu.a[2] = 0x55667788u;
    ctx.cpu.pc = 0x2222u; ctx.cpu.sr = 0x0010u;
    setup_parent_pexec(memory, &ctx); parent_cpu = ctx.cpu;

    rc = amtari_gemdos_dispatch(&ctx, 0x4bu);
    assert(rc == 42);
    assert(fixture.child_count == 1); assert(fixture.grand_count == 1);
    assert(ctx.process_depth == 0u); assert(ctx.current_basepage == 0x0800u);
    assert(ctx.next_load_address == 0x1000u);
    assert(ctx.heap_top == 0u);
    assert(memcmp(&ctx.cpu, &parent_cpu, sizeof(parent_cpu)) == 0);

    assert(get32(memory, 0x1004u) == 0x2140u);
    assert(get32(memory, 0x2144u) == 0x3250u);

    rc = amtari_trap_dispatch(&ctx, 1u, 0x4bu);
    assert(rc == 42);
    assert(fixture.child_count == 2); assert(fixture.grand_count == 2);
    assert(ctx.process_depth == 0u); assert(ctx.next_load_address == 0x1000u);
    assert(ctx.heap_top == 0u);
    assert(memcmp(&ctx.cpu, &parent_cpu, sizeof(parent_cpu)) == 0);

    ctx.process_depth = 8u; setup_parent_pexec(memory, &ctx);
    assert(amtari_gemdos_dispatch(&ctx, 0x4bu) == AMTARI_ENOSYS);
    assert(fixture.child_count == 2); assert(fixture.grand_count == 2);
    ctx.process_depth = 0u;

    /* M2.19 GEMDOS heap: allocations are 16-byte aligned and begin at the
     * current process/load high-water mark. */
    a = call_malloc(memory, &ctx, 32u);
    b = call_malloc(memory, &ctx, 48u);
    assert(a == 0x1000);
    assert(b == 0x1020);
    assert(ctx.heap_top == 0x1050u);

    /* Free/reuse an exact block. This initial allocator intentionally reuses
     * whole free blocks rather than splitting them on Malloc. */
    assert(call_mfree(memory, &ctx, (uint32_t)a) == 0);
    c = call_malloc(memory, &ctx, 16u);
    assert(c == a);

    /* Shrinking a tail allocation lowers the high-water mark. */
    assert(call_mshrink(memory, &ctx, (uint32_t)b, 16u) == 0);
    assert(ctx.heap_top == 0x1030u);
    assert(call_malloc(memory, &ctx, 32u) == 0x1030);
    assert(ctx.heap_top == 0x1050u);

    /* Query-largest and invalid-free behavior are deterministic. */
    assert(call_malloc(memory, &ctx, UINT32_MAX) > 0);
    assert(call_mfree(memory, &ctx, 0x7770u) == AMTARI_EINVAL);

    return 0;
}
