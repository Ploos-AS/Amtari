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

static int32_t gemdos_dsetdrv(struct amtari_context *ctx)
{
    uint16_t drive;

    if (read_arg16(ctx, 0, &drive) != 0) {
        return AMTARI_EFAULT;
    }
    if (drive >= 26u || (ctx->drive_mask & (1u << drive)) == 0u) {
        return AMTARI_ENOENT;
    }

    ctx->current_drive = (uint8_t)drive;
    return (int32_t)ctx->drive_mask;
}

static int32_t gemdos_dgetdrv(const struct amtari_context *ctx)
{
    return (int32_t)ctx->current_drive;
}

static int32_t gemdos_fopen(struct amtari_context *ctx)
{
    uint32_t filename;
    uint16_t mode;
    char path[AMTARI_PATH_MAX];
    int rc;

    if (ctx->fs.open == 0) {
        return AMTARI_EIO;
    }
    if (read_arg32(ctx, 0, &filename) != 0 || read_arg16(ctx, 4, &mode) != 0) {
        return AMTARI_EFAULT;
    }
    rc = amtari_path_translate(ctx, filename, path, sizeof(path));
    if (rc != 0) {
        return rc;
    }

    return ctx->fs.open(ctx->fs.opaque, path, mode);
}

static int32_t gemdos_fclose(struct amtari_context *ctx)
{
    uint16_t handle;

    if (ctx->fs.close == 0) {
        return AMTARI_EIO;
    }
    if (read_arg16(ctx, 0, &handle) != 0) {
        return AMTARI_EFAULT;
    }

    return ctx->fs.close(ctx->fs.opaque, (int16_t)handle);
}

static int32_t gemdos_fread(struct amtari_context *ctx)
{
    uint16_t handle;
    uint32_t count;
    uint32_t buffer;

    if (ctx->fs.read == 0) {
        return AMTARI_EIO;
    }
    if (read_arg16(ctx, 0, &handle) != 0 || read_arg32(ctx, 2, &count) != 0 ||
        read_arg32(ctx, 6, &buffer) != 0) {
        return AMTARI_EFAULT;
    }
    if (!amtari_guest_range_valid(ctx, buffer, (size_t)count)) {
        return AMTARI_EFAULT;
    }

    return ctx->fs.read(ctx->fs.opaque, (int16_t)handle, &ctx->memory.data[buffer], count);
}

static int32_t gemdos_fwrite(struct amtari_context *ctx)
{
    uint16_t handle;
    uint32_t count;
    uint32_t buffer;

    if (ctx->fs.write == 0) {
        return AMTARI_EIO;
    }
    if (read_arg16(ctx, 0, &handle) != 0 || read_arg32(ctx, 2, &count) != 0 ||
        read_arg32(ctx, 6, &buffer) != 0) {
        return AMTARI_EFAULT;
    }
    if (!amtari_guest_range_valid(ctx, buffer, (size_t)count)) {
        return AMTARI_EFAULT;
    }

    return ctx->fs.write(ctx->fs.opaque, (int16_t)handle, &ctx->memory.data[buffer], count);
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
    case 0x0e:
        return gemdos_dsetdrv(ctx);
    case 0x19:
        return gemdos_dgetdrv(ctx);
    case 0x3d:
        return gemdos_fopen(ctx);
    case 0x3e:
        return gemdos_fclose(ctx);
    case 0x3f:
        return gemdos_fread(ctx);
    case 0x40:
        return gemdos_fwrite(ctx);
    default:
        return AMTARI_ENOSYS;
    }
}
