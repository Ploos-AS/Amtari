#include "amtari.h"

static int read_arg16(const struct amtari_context *ctx, uint32_t offset, uint16_t *value)
{
    return amtari_guest_read16(ctx, ctx->cpu.a[7] + 2u + offset, value);
}

static int read_arg32(const struct amtari_context *ctx, uint32_t offset, uint32_t *value)
{
    return amtari_guest_read32(ctx, ctx->cpu.a[7] + 2u + offset, value);
}

int amtari_console_bind(struct amtari_context *ctx, amtari_console_getc_fn getc_fn,
                        amtari_console_putc_fn putc_fn, void *opaque)
{
    if (ctx == 0) {
        return AMTARI_EINVAL;
    }

    ctx->console.getc = getc_fn;
    ctx->console.putc = putc_fn;
    ctx->console.opaque = opaque;
    return 0;
}

static int32_t gemdos_cconin(struct amtari_context *ctx)
{
    int value;

    if (ctx->console.getc == 0) {
        return AMTARI_EIO;
    }

    value = ctx->console.getc(ctx->console.opaque);
    if (value < 0) {
        return AMTARI_EIO;
    }

    return (int32_t)(value & 0xff);
}

static int32_t gemdos_cconout(struct amtari_context *ctx)
{
    uint16_t value;

    if (ctx->console.putc == 0) {
        return AMTARI_EIO;
    }
    if (read_arg16(ctx, 0, &value) != 0) {
        return AMTARI_EFAULT;
    }
    if (ctx->console.putc(ctx->console.opaque, (unsigned char)(value & 0xffu)) < 0) {
        return AMTARI_EIO;
    }

    return 0;
}

static int32_t gemdos_cconws(struct amtari_context *ctx)
{
    uint32_t address;

    if (ctx->console.putc == 0) {
        return AMTARI_EIO;
    }
    if (read_arg32(ctx, 0, &address) != 0) {
        return AMTARI_EFAULT;
    }

    for (;;) {
        unsigned char ch;

        if (!amtari_guest_range_valid(ctx, address, 1)) {
            return AMTARI_EFAULT;
        }
        ch = ctx->memory.data[address++];
        if (ch == 0) {
            break;
        }
        if (ctx->console.putc(ctx->console.opaque, ch) < 0) {
            return AMTARI_EIO;
        }
    }

    return 0;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    if (ctx == 0 || !ctx->initialized) {
        return AMTARI_EINVAL;
    }

    switch (function) {
    case 0x01:
        return gemdos_cconin(ctx);
    case 0x02:
        return gemdos_cconout(ctx);
    case 0x09:
        return gemdos_cconws(ctx);
    default:
        return AMTARI_ENOSYS;
    }
}
