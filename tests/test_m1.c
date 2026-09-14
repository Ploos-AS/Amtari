#include <assert.h>
#include <stdint.h>

#include "amtari.h"

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

int main(void)
{
    test_guest_memory();
    test_traps();
    return 0;
}
