#include "amtari.h"

int amtari_guest_memory_bind(struct amtari_context *ctx, uint8_t *data, size_t size)
{
    if (ctx == 0 || data == 0 || size == 0) {
        return AMTARI_EINVAL;
    }

    ctx->memory.data = data;
    ctx->memory.size = size;
    return 0;
}

int amtari_guest_range_valid(const struct amtari_context *ctx, uint32_t address, size_t length)
{
    size_t start;

    if (ctx == 0 || ctx->memory.data == 0) {
        return 0;
    }

    start = (size_t)address;
    if (start > ctx->memory.size) {
        return 0;
    }
    if (length > ctx->memory.size - start) {
        return 0;
    }

    return 1;
}

int amtari_guest_read16(const struct amtari_context *ctx, uint32_t address, uint16_t *value)
{
    const uint8_t *p;

    if (value == 0 || !amtari_guest_range_valid(ctx, address, 2)) {
        return AMTARI_EFAULT;
    }

    p = &ctx->memory.data[address];
    *value = (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
    return 0;
}

int amtari_guest_read32(const struct amtari_context *ctx, uint32_t address, uint32_t *value)
{
    const uint8_t *p;

    if (value == 0 || !amtari_guest_range_valid(ctx, address, 4)) {
        return AMTARI_EFAULT;
    }

    p = &ctx->memory.data[address];
    *value = ((uint32_t)p[0] << 24) |
             ((uint32_t)p[1] << 16) |
             ((uint32_t)p[2] << 8) |
             (uint32_t)p[3];
    return 0;
}
