#include <assert.h>
#include <stdint.h>

#include "amtari.h"

struct console_fixture {
    int input;
    int input_ready;
    int output_ready;
    int output_count;
    unsigned char output;
};

static int console_getc(void *opaque)
{
    struct console_fixture *fixture = (struct console_fixture *)opaque;
    return fixture->input;
}

static int console_putc(void *opaque, unsigned char ch)
{
    struct console_fixture *fixture = (struct console_fixture *)opaque;
    fixture->output = ch;
    ++fixture->output_count;
    return 0;
}

static int console_input_ready(void *opaque)
{
    struct console_fixture *fixture = (struct console_fixture *)opaque;
    return fixture->input_ready;
}

static int console_output_ready(void *opaque)
{
    struct console_fixture *fixture = (struct console_fixture *)opaque;
    return fixture->output_ready;
}

static void put16(uint8_t *memory, uint32_t address, uint16_t value)
{
    memory[address] = (uint8_t)(value >> 8);
    memory[address + 1u] = (uint8_t)value;
}

static void test_guest_memory(void)
{
    struct amtari_context ctx = {0};
    uint8_t memory[8] = {0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0};
    uint16_t value16 = 0;
    uint32_t value32 = 0;

    assert(amtari_init(&ctx) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_guest_range_valid(&ctx, 0, 8) == 1);
    assert(amtari_guest_range_valid(&ctx, 7, 2) == 0);
    assert(amtari_guest_read16(&ctx, 0, &value16) == 0);
    assert(value16 == 0x1234u);
    assert(amtari_guest_read32(&ctx, 0, &value32) == 0);
    assert(value32 == 0x12345678u);
    assert(amtari_guest_read32(&ctx, 6, &value32) == AMTARI_EFAULT);
}

static void test_traps(void)
{
    struct amtari_context ctx = {0};

    assert(amtari_trap_decode(1) == AMTARI_TRAP_GEMDOS);
    assert(amtari_trap_decode(13) == AMTARI_TRAP_BIOS);
    assert(amtari_trap_decode(14) == AMTARI_TRAP_XBIOS);
    assert(amtari_trap_decode(2) == AMTARI_TRAP_UNKNOWN);

    assert(amtari_trap_dispatch(&ctx, 1, 0xffffu) == AMTARI_EINVAL);
    assert(amtari_init(&ctx) == 0);
    assert(amtari_trap_dispatch(&ctx, 1, 0xffffu) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 13, 0) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 14, 0) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 2, 0) == AMTARI_ENOSYS);
}

static void test_bios_console_and_drvmap(void)
{
    struct amtari_context ctx = {0};
    struct console_fixture fixture = {0x41, 1, 1, 0, 0};
    uint8_t memory[64] = {0};

    assert(amtari_init(&ctx) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_console_bind(&ctx, console_getc, console_putc, &fixture) == 0);
    assert(amtari_console_status_bind(&ctx, console_input_ready, console_output_ready) == 0);

    ctx.cpu.a[7] = 0x10u;

    put16(memory, 0x10u, 0x0001u);
    put16(memory, 0x12u, 0x0002u);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x01u) == -1);
    fixture.input_ready = 0;
    assert(amtari_trap_dispatch(&ctx, 13u, 0x01u) == 0);
    fixture.input_ready = 1;

    put16(memory, 0x10u, 0x0002u);
    put16(memory, 0x12u, 0x0002u);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x02u) == 0x41);

    put16(memory, 0x10u, 0x0003u);
    put16(memory, 0x12u, 0x0002u);
    put16(memory, 0x14u, 0x005au);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x03u) == 0);
    assert(fixture.output_count == 1);
    assert(fixture.output == (unsigned char)'Z');

    put16(memory, 0x10u, 0x0008u);
    put16(memory, 0x12u, 0x0002u);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x08u) == -1);
    fixture.output_ready = 0;
    assert(amtari_trap_dispatch(&ctx, 13u, 0x08u) == 0);
    fixture.output_ready = 1;

    put16(memory, 0x12u, 0x0001u);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x01u) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x02u) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x03u) == AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x08u) == AMTARI_ENOSYS);

    assert(amtari_fs_set_drives(&ctx, 0x00000005u, 0u) == 0);
    assert(amtari_trap_dispatch(&ctx, 13u, 0x0au) == 5);
}

int main(void)
{
    test_guest_memory();
    test_traps();
    test_bios_console_and_drvmap();
    return 0;
}
