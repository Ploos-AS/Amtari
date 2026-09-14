#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amtari.h"

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *fp;
    long length;
    uint8_t *data;

    fp = fopen(path, "rb");
    if (fp == 0) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    length = ftell(fp);
    if (length <= 0) { fclose(fp); return 0; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return 0; }

    data = (uint8_t *)malloc((size_t)length);
    if (data == 0) { fclose(fp); return 0; }
    if (fread(data, 1u, (size_t)length, fp) != (size_t)length) {
        free(data);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *size_out = (size_t)length;
    return data;
}

int main(int argc, char **argv)
{
    struct amtari_context ctx = {0};
    struct amtari_prg_info info;
    uint8_t memory[65536] = {0};
    uint8_t *image;
    size_t image_size = 0u;
    uint32_t steps = 0u;
    uint32_t expected;
    int32_t basepage;
    int rc;

    assert(argc == 3);
    expected = (uint32_t)strtoul(argv[2], 0, 0);
    assert(amtari_init(&ctx) == 0);
    assert(strncmp(amtari_version(), "0.2.", 4u) == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);

    image = read_file(argv[1], &image_size);
    assert(image != 0);
    assert(amtari_prg_parse(image, image_size, &info) == 0);
    assert(info.text_size > 0u);

    basepage = amtari_prg_load(&ctx, image, image_size, 0x1000u, 0);
    assert(basepage == 0x1000);
    free(image);

    assert(amtari_exec_prepare(&ctx, (uint32_t)basepage, 0xf000u) == 0);
    rc = amtari_exec_run(&ctx, 16384u, &steps);
    if (rc == AMTARI_EILLEGAL) {
        uint16_t opcode = 0u;
        (void)amtari_guest_read16(&ctx, ctx.cpu.pc, &opcode);
        fprintf(stderr, "cross PRG illegal opcode 0x%04x at PC=0x%08lx after %lu steps\n",
                (unsigned int)opcode, (unsigned long)ctx.cpu.pc, (unsigned long)steps);
    }
    assert(rc == AMTARI_EXEC_HALTED);
    assert(ctx.cpu.d[0] == expected);
    assert(steps > 0u);

    return 0;
}
