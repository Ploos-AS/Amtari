#include <string.h>

#include "amtari.h"

#define PRG_HEADER_SIZE 28u

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t align16(uint32_t value)
{
    return (value + 15u) & ~15u;
}

int amtari_program_bind(struct amtari_context *ctx, amtari_program_fetch_fn fetch_fn, void *opaque)
{
    if (ctx == 0) {
        return AMTARI_EINVAL;
    }
    ctx->process.fetch = fetch_fn;
    ctx->process.opaque = opaque;
    return 0;
}

int amtari_process_set_load_address(struct amtari_context *ctx, uint32_t address)
{
    if (ctx == 0) {
        return AMTARI_EINVAL;
    }
    ctx->next_load_address = address;
    return 0;
}

int amtari_prg_parse(const uint8_t *image, size_t image_size, struct amtari_prg_info *info)
{
    uint64_t minimum;

    if (image == 0 || info == 0 || image_size < PRG_HEADER_SIZE) {
        return AMTARI_EINVAL;
    }
    if (be16(image) != 0x601au) {
        return AMTARI_ENOEXEC;
    }

    info->text_size = be32(&image[2]);
    info->data_size = be32(&image[6]);
    info->bss_size = be32(&image[10]);
    info->symbol_size = be32(&image[14]);
    info->flags = be32(&image[22]);
    info->absolute = be16(&image[26]);

    minimum = (uint64_t)PRG_HEADER_SIZE + info->text_size + info->data_size + info->symbol_size;
    if (minimum > image_size) {
        return AMTARI_ENOEXEC;
    }
    return 0;
}

static int relocate(struct amtari_context *ctx, const uint8_t *image, size_t image_size,
                    const struct amtari_prg_info *info, uint32_t tbase)
{
    size_t pos = PRG_HEADER_SIZE + (size_t)info->text_size + (size_t)info->data_size +
                 (size_t)info->symbol_size;
    uint32_t offset;

    if (info->absolute != 0u) {
        return 0;
    }
    if (pos + 4u > image_size) {
        return AMTARI_ENOEXEC;
    }

    offset = be32(&image[pos]);
    pos += 4u;
    if (offset == 0u) {
        return 0;
    }

    for (;;) {
        uint32_t address = tbase + offset;
        uint32_t value;
        uint8_t delta;

        if (!amtari_guest_range_valid(ctx, address, 4u)) {
            return AMTARI_EFAULT;
        }
        value = be32(&ctx->memory.data[address]);
        put32(&ctx->memory.data[address], value + tbase);

        if (pos >= image_size) {
            return AMTARI_ENOEXEC;
        }
        delta = image[pos++];
        if (delta == 0u) {
            return 0;
        }
        if (delta == 1u) {
            offset += 254u;
        } else {
            offset += delta;
        }
    }
}

int32_t amtari_prg_load_reserved(struct amtari_context *ctx, const uint8_t *image, size_t image_size,
                                uint32_t basepage, const uint8_t *cmdline, uint32_t tail_reserve)
{
    struct amtari_prg_info info;
    uint32_t tbase;
    uint32_t dbase;
    uint32_t bbase;
    uint32_t image_end;
    uint32_t allocation_top;
    uint64_t image_total;
    uint64_t allocation_total;
    size_t command_length = 0u;
    int rc;

    if (ctx == 0 || ctx->memory.data == 0) {
        return AMTARI_EINVAL;
    }
    rc = amtari_prg_parse(image, image_size, &info);
    if (rc != 0) {
        return rc;
    }

    tbase = basepage + AMTARI_BASEPAGE_SIZE;
    dbase = tbase + info.text_size;
    bbase = dbase + info.data_size;
    image_total = (uint64_t)AMTARI_BASEPAGE_SIZE + info.text_size + info.data_size + info.bss_size;
    if (image_total > UINT32_MAX) {
        return AMTARI_ENOMEM;
    }
    image_end = basepage + (uint32_t)image_total;
    if (image_end < basepage) {
        return AMTARI_ENOMEM;
    }

    allocation_top = align16(image_end);
    if (allocation_top < image_end || allocation_top > UINT32_MAX - tail_reserve) {
        return AMTARI_ENOMEM;
    }
    allocation_top += tail_reserve;
    allocation_top = align16(allocation_top);
    if (allocation_top < basepage) {
        return AMTARI_ENOMEM;
    }
    allocation_total = (uint64_t)allocation_top - basepage;
    if (allocation_total > SIZE_MAX ||
        !amtari_guest_range_valid(ctx, basepage, (size_t)allocation_total)) {
        return AMTARI_ENOMEM;
    }

    memset(&ctx->memory.data[basepage], 0, (size_t)allocation_total);
    memcpy(&ctx->memory.data[tbase], &image[PRG_HEADER_SIZE], info.text_size + info.data_size);

    put32(&ctx->memory.data[basepage + 0x00u], basepage);
    put32(&ctx->memory.data[basepage + 0x04u], allocation_top);
    put32(&ctx->memory.data[basepage + 0x08u], tbase);
    put32(&ctx->memory.data[basepage + 0x0cu], info.text_size);
    put32(&ctx->memory.data[basepage + 0x10u], dbase);
    put32(&ctx->memory.data[basepage + 0x14u], info.data_size);
    put32(&ctx->memory.data[basepage + 0x18u], bbase);
    put32(&ctx->memory.data[basepage + 0x1cu], info.bss_size);
    put32(&ctx->memory.data[basepage + 0x20u], basepage + 0x80u);
    put32(&ctx->memory.data[basepage + 0x24u], ctx->current_basepage);

    if (cmdline != 0) {
        command_length = cmdline[0];
        if (command_length > 124u) {
            command_length = 124u;
        }
        ctx->memory.data[basepage + 0x80u] = (uint8_t)command_length;
        memcpy(&ctx->memory.data[basepage + 0x81u], &cmdline[1], command_length);
    }

    rc = relocate(ctx, image, image_size, &info, tbase);
    if (rc != 0) {
        return rc;
    }

    ctx->next_load_address = allocation_top;
    return (int32_t)basepage;
}

int32_t amtari_prg_load(struct amtari_context *ctx, const uint8_t *image, size_t image_size,
                        uint32_t basepage, const uint8_t *cmdline)
{
    return amtari_prg_load_reserved(ctx, image, image_size, basepage, cmdline, 0u);
}
