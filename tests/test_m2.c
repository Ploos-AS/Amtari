#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

struct console_fixture {
    unsigned char out[64];
    size_t out_len;
    int input;
};

static int fixture_getc(void *opaque)
{
    struct console_fixture *fixture = opaque;
    return fixture->input;
}

static int fixture_putc(void *opaque, unsigned char ch)
{
    struct console_fixture *fixture = opaque;

    if (fixture->out_len >= sizeof(fixture->out)) {
        return -1;
    }
    fixture->out[fixture->out_len++] = ch;
    return 0;
}

static void put16(uint8_t *memory, uint32_t address, uint16_t value)
{
    memory[address] = (uint8_t)(value >> 8);
    memory[address + 1] = (uint8_t)value;
}

static void put32(uint8_t *memory, uint32_t address, uint32_t value)
{
    memory[address] = (uint8_t)(value >> 24);
    memory[address + 1] = (uint8_t)(value >> 16);
    memory[address + 2] = (uint8_t)(value >> 8);
    memory[address + 3] = (uint8_t)value;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct console_fixture console = {{0}, 0, 'Z'};
    uint8_t memory[256] = {0};
    static const unsigned char message[] = "Amtari";

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), AMTARI_VERSION) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_console_bind(&ctx, fixture_getc, fixture_putc, &console) == 0);

    ctx.cpu.a[7] = 32;

    /* GEMDOS Cconin (0x01). */
    assert(amtari_trap_dispatch(&ctx, 1, 0x01) == 'Z');

    /* GEMDOS Cconout (0x02): function word is at SP, argument follows it. */
    put16(memory, ctx.cpu.a[7] + 2, 'A');
    assert(amtari_trap_dispatch(&ctx, 1, 0x02) == 0);
    assert(console.out_len == 1);
    assert(console.out[0] == 'A');

    /* GEMDOS Cconws (0x09): pointer argument follows function word. */
    memcpy(&memory[128], message, sizeof(message));
    put32(memory, ctx.cpu.a[7] + 2, 128);
    assert(amtari_trap_dispatch(&ctx, 1, 0x09) == 0);
    assert(console.out_len == 1 + sizeof(message) - 1);
    assert(memcmp(&console.out[1], message, sizeof(message) - 1) == 0);

    /* Invalid Cconws pointer must fail rather than escape guest memory. */
    put32(memory, ctx.cpu.a[7] + 2, 255);
    memory[255] = 'X';
    assert(amtari_trap_dispatch(&ctx, 1, 0x09) == AMTARI_EFAULT);

    assert(amtari_trap_dispatch(&ctx, 1, 0x7fff) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 13, 0x01) == AMTARI_ENOSYS);

    return 0;
}
