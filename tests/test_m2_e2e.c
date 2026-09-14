#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

struct program_fixture {
    const uint8_t *image;
    size_t size;
    char last_path[AMTARI_PATH_MAX];
};

struct console_fixture {
    unsigned char out[64];
    size_t out_len;
};

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int fetch_program(void *opaque, const char *path,
                         const uint8_t **data, size_t *size)
{
    struct program_fixture *fixture = opaque;
    strcpy(fixture->last_path, path);
    *data = fixture->image;
    *size = fixture->size;
    return 0;
}

static int console_putc(void *opaque, unsigned char ch)
{
    struct console_fixture *fixture = opaque;
    if (fixture->out_len >= sizeof(fixture->out)) return -1;
    fixture->out[fixture->out_len++] = ch;
    return 0;
}

static size_t make_hello_prg(uint8_t *image)
{
    static const unsigned char message[12] = "Amtari PRG!";

    memset(image, 0, 80u);
    put16(&image[0], 0x601au);
    put32(&image[2], 16u);
    put32(&image[6], 12u);
    put32(&image[10], 64u);
    put32(&image[14], 0u);
    put32(&image[22], 0u);
    put16(&image[26], 0u);

    put16(&image[28], 0x2f3cu);
    put32(&image[30], 16u);
    put16(&image[34], 0x3f3cu);
    put16(&image[36], 0x0009u);
    put16(&image[38], 0x4e41u);
    put16(&image[40], 0x5c8fu);
    put16(&image[42], 0x4e75u);
    memcpy(&image[44], message, sizeof(message));
    put32(&image[56], 2u);
    image[60] = 0u;
    return 61u;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct program_fixture program;
    struct console_fixture console = {{0}, 0u};
    uint8_t memory[32768] = {0};
    uint8_t image[80];
    uint32_t steps = 0u;
    uint32_t tbase;
    uint32_t hitpa;
    int32_t basepage;

    program.size = make_hello_prg(image);
    program.image = image;
    program.last_path[0] = '\0';

    assert(amtari_init(&ctx) == 0);
    assert(strncmp(amtari_version(), "0.2.", 4) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_console_bind(&ctx, 0, console_putc, &console) == 0);
    assert(amtari_program_bind(&ctx, fetch_program, &program) == 0);
    assert(amtari_fs_set_drives(&ctx, 1u, 0u) == 0);
    assert(amtari_process_set_load_address(&ctx, 0x1000u) == 0);

    strcpy((char *)&memory[0x200u], "A:\\HELLO.PRG");
    ctx.cpu.a[7] = 0x300u;
    put16(&memory[0x302u], 3u);
    put32(&memory[0x304u], 0x200u);
    put32(&memory[0x308u], 0u);
    put32(&memory[0x30cu], 0u);
    basepage = amtari_gemdos_dispatch(&ctx, 0x4bu);
    assert(basepage == 0x1000);
    assert(strcmp(program.last_path, "A:/HELLO.PRG") == 0);

    tbase = get32(&memory[(uint32_t)basepage + 0x08u]);
    hitpa = get32(&memory[(uint32_t)basepage + 0x04u]);
    assert(tbase == 0x1100u);
    assert(hitpa == 0x115cu);
    assert(get32(&memory[tbase + 2u]) == tbase + 16u);
    assert(memcmp(&memory[tbase + 16u], "Amtari PRG!", 11u) == 0);

    ctx.cpu.a[7] = 0x300u;
    put16(&memory[0x302u], 4u);
    put32(&memory[0x304u], 0u);
    put32(&memory[0x308u], (uint32_t)basepage);
    put32(&memory[0x30cu], 0u);
    assert(amtari_gemdos_dispatch(&ctx, 0x4bu) == 0);
    assert(ctx.current_basepage == (uint32_t)basepage);
    assert(ctx.cpu.pc == tbase);
    assert(ctx.cpu.a[0] == (uint32_t)basepage);
    assert(ctx.cpu.a[7] == hitpa - 4u);

    assert(amtari_exec_run(&ctx, 32u, &steps) == AMTARI_EXEC_HALTED);
    assert(steps == 5u);
    assert(console.out_len == 11u);
    assert(memcmp(console.out, "Amtari PRG!", 11u) == 0);
    assert(ctx.cpu.a[7] == hitpa);

    return 0;
}
