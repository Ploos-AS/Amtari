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

struct clock_fixture {
    uint32_t value;
    uint32_t set_value;
    int fail;
    int set_fail;
    int set_count;
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

static int clock_get(void *opaque, uint32_t *tos_datetime)
{
    struct clock_fixture *fixture = (struct clock_fixture *)opaque;
    if (fixture->fail) return -1;
    *tos_datetime = fixture->value;
    return 0;
}

static int clock_set(void *opaque, uint32_t tos_datetime)
{
    struct clock_fixture *fixture = (struct clock_fixture *)opaque;
    if (fixture->set_fail) return -1;
    fixture->set_value = tos_datetime;
    ++fixture->set_count;
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

static void test_xbios_random(void)
{
    struct amtari_context ctx = {0};
    int32_t first;
    int32_t second;
    int32_t third;

    assert(amtari_init(&ctx) == 0);
    assert(amtari_random_seed(&ctx, 1u) == 0);

    first = amtari_trap_dispatch(&ctx, 14u, 0x11u);
    second = amtari_trap_dispatch(&ctx, 14u, 0x11u);
    third = amtari_trap_dispatch(&ctx, 14u, 0x11u);

    assert(first == 0x00bb40e6);
    assert(second == 0x005eb5ca);
    assert(third == 0x004c4530);
    assert((first & ~0x00ffffff) == 0);
    assert((second & ~0x00ffffff) == 0);
    assert((third & ~0x00ffffff) == 0);

    assert(amtari_random_seed(&ctx, 1u) == 0);
    assert(amtari_trap_dispatch(&ctx, 14u, 0x11u) == first);
}

static void test_xbios_time(void)
{
    struct amtari_context ctx = {0};
    struct clock_fixture fixture = {UINT32_C(0x5c4f7b1d), 0u, 0, 0, 0};
    uint8_t memory[64] = {0};

    assert(amtari_init(&ctx) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_trap_dispatch(&ctx, 14u, 0x17u) == AMTARI_EIO);
    assert(amtari_clock_bind(&ctx, clock_get, &fixture) == 0);
    assert((uint32_t)amtari_trap_dispatch(&ctx, 14u, 0x17u) == fixture.value);

    fixture.value = UINT32_C(0xfc4f7b1d);
    assert((uint32_t)amtari_trap_dispatch(&ctx, 14u, 0x17u) == fixture.value);

    ctx.cpu.a[7] = 0x10u;
    put16(memory, 0x10u, 0x0016u);
    put32(memory, 0x12u, UINT32_C(0x9abcdef0));
    assert(amtari_trap_dispatch(&ctx, 14u, 0x16u) == AMTARI_EIO);

    assert(amtari_clock_bind_rw(&ctx, clock_get, clock_set, &fixture) == 0);
    assert(amtari_trap_dispatch(&ctx, 14u, 0x16u) == 0);
    assert(fixture.set_count == 1);
    assert(fixture.set_value == UINT32_C(0x9abcdef0));

    fixture.set_fail = 1;
    assert(amtari_trap_dispatch(&ctx, 14u, 0x16u) == AMTARI_EIO);
    fixture.set_fail = 0;

    ctx.cpu.a[7] = 62u;
    assert(amtari_trap_dispatch(&ctx, 14u, 0x16u) == AMTARI_EFAULT);

    fixture.fail = 1;
    assert(amtari_trap_dispatch(&ctx, 14u, 0x17u) == AMTARI_EIO);
}

int main(void)
{
    test_guest_memory();
    test_traps();
    test_bios_console_and_drvmap();
    test_xbios_random();
    test_xbios_time();
    return 0;
}
