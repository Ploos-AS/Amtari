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

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int fetch_program(void *opaque, const char *path, const uint8_t **data, size_t *size)
{
    struct program_fixture *fixture = opaque;
    strcpy(fixture->last_path, path);
    *data = fixture->image;
    *size = fixture->size;
    return 0;
}

static size_t make_reloc_prg(uint8_t *image)
{
    memset(image, 0, 64);
    put16(&image[0], 0x601a);
    put32(&image[2], 8);  /* text */
    put32(&image[6], 4);  /* data */
    put32(&image[10], 8); /* bss */
    put32(&image[14], 0); /* symbols */
    put32(&image[22], 0); /* flags */
    put16(&image[26], 0); /* relocatable */

    /* TEXT starts at file offset 28. Relocate long at text offset 4. */
    image[28] = 0x4e;
    image[29] = 0x71;
    image[30] = 0x4e;
    image[31] = 0x71;
    put32(&image[32], 0x00000008u);
    image[36] = 'D';
    image[37] = 'A';
    image[38] = 'T';
    image[39] = 'A';

    put32(&image[40], 4u); /* first relocation offset */
    image[44] = 0u;       /* end */
    return 45u;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct amtari_prg_info info;
    struct program_fixture fixture;
    uint8_t memory[16384] = {0};
    uint8_t image[64];
    size_t image_size;
    int32_t basepage;
    uint32_t tbase;

    image_size = make_reloc_prg(image);
    fixture.image = image;
    fixture.size = image_size;
    fixture.last_path[0] = '\0';

    assert(amtari_prg_parse(image, image_size, &info) == 0);
    assert(info.text_size == 8u);
    assert(info.data_size == 4u);
    assert(info.bss_size == 8u);
    assert(info.absolute == 0u);

    image[0] = 0;
    assert(amtari_prg_parse(image, image_size, &info) == AMTARI_ENOEXEC);
    image[0] = 0x60;

    assert(amtari_init(&ctx) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_fs_set_drives(&ctx, 1u, 0u) == 0);
    assert(amtari_program_bind(&ctx, fetch_program, &fixture) == 0);
    assert(amtari_process_set_load_address(&ctx, 0x1000u) == 0);

    strcpy((char *)&memory[0x200], "A:\\HELLO.PRG");
    memory[0x240] = 3;
    memcpy(&memory[0x241], "-v ", 3);

    /* Pexec(3, name, cmdline, env): load, don't go. */
    ctx.cpu.a[7] = 0x100u;
    put16(&memory[0x102], 3u);
    put32(&memory[0x104], 0x200u);
    put32(&memory[0x108], 0x240u);
    put32(&memory[0x10c], 0u);
    basepage = amtari_gemdos_dispatch(&ctx, 0x4b);
    assert(basepage == 0x1000);
    assert(strcmp(fixture.last_path, "A:/HELLO.PRG") == 0);

    tbase = 0x1000u + AMTARI_BASEPAGE_SIZE;
    assert(get32(&memory[0x1000]) == 0x1000u);
    assert(get32(&memory[0x1008]) == tbase);
    assert(get32(&memory[0x100c]) == 8u);
    assert(get32(&memory[0x1010]) == tbase + 8u);
    assert(get32(&memory[0x1018]) == tbase + 12u);
    assert(memory[0x1080] == 3u);
    assert(memcmp(&memory[0x1081], "-v ", 3) == 0);
    assert(get32(&memory[tbase + 4u]) == tbase + 8u);
    assert(memory[tbase + 12u] == 0u);
    assert(memory[tbase + 19u] == 0u);

    /* Pexec(4, 0, basepage, 0): prepare execution state. */
    put16(&memory[0x102], 4u);
    put32(&memory[0x104], 0u);
    put32(&memory[0x108], (uint32_t)basepage);
    put32(&memory[0x10c], 0u);
    assert(amtari_gemdos_dispatch(&ctx, 0x4b) == 0);
    assert(ctx.current_basepage == (uint32_t)basepage);
    assert(ctx.cpu.pc == tbase);

    /* Unsupported Pexec modes must not pretend to execute yet. */
    put16(&memory[0x102], 0u);
    assert(amtari_gemdos_dispatch(&ctx, 0x4b) == AMTARI_ENOSYS);

    return 0;
}
