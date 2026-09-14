#include <string.h>

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

static int path_arg(struct amtari_context *ctx, uint32_t offset, char *path, size_t path_size)
{
    uint32_t address;
    int rc;

    if (read_arg32(ctx, offset, &address) != 0) {
        return AMTARI_EFAULT;
    }
    rc = amtari_path_translate(ctx, address, path, path_size);
    return rc;
}

static int32_t gemdos_cconin(struct amtari_context *ctx)
{
    int value;

    if (ctx->console.getc == 0) return AMTARI_EIO;
    value = ctx->console.getc(ctx->console.opaque);
    return value < 0 ? AMTARI_EIO : (int32_t)(value & 0xff);
}

static int32_t gemdos_cconout(struct amtari_context *ctx)
{
    uint16_t value;
    if (ctx->console.putc == 0) return AMTARI_EIO;
    if (read_arg16(ctx, 0, &value) != 0) return AMTARI_EFAULT;
    return ctx->console.putc(ctx->console.opaque, (unsigned char)(value & 0xffu)) < 0 ? AMTARI_EIO : 0;
}

static int32_t gemdos_cconws(struct amtari_context *ctx)
{
    uint32_t address;
    if (ctx->console.putc == 0) return AMTARI_EIO;
    if (read_arg32(ctx, 0, &address) != 0) return AMTARI_EFAULT;
    for (;;) {
        unsigned char ch;
        if (!amtari_guest_range_valid(ctx, address, 1)) return AMTARI_EFAULT;
        ch = ctx->memory.data[address++];
        if (ch == 0) break;
        if (ctx->console.putc(ctx->console.opaque, ch) < 0) return AMTARI_EIO;
    }
    return 0;
}

static int32_t gemdos_dsetdrv(struct amtari_context *ctx)
{
    uint16_t drive;
    if (read_arg16(ctx, 0, &drive) != 0) return AMTARI_EFAULT;
    if (drive >= 26u || (ctx->drive_mask & (1u << drive)) == 0u) return AMTARI_ENOENT;
    ctx->current_drive = (uint8_t)drive;
    return (int32_t)ctx->drive_mask;
}

static int32_t gemdos_dgetdrv(const struct amtari_context *ctx)
{
    return (int32_t)ctx->current_drive;
}

static int32_t gemdos_dcreate(struct amtari_context *ctx)
{
    char path[AMTARI_PATH_MAX];
    int rc;
    if (ctx->fs.mkdir == 0) return AMTARI_EIO;
    rc = path_arg(ctx, 0, path, sizeof(path));
    return rc != 0 ? rc : amtari_gemdos_error_from_host(ctx->fs.mkdir(ctx->fs.opaque, path));
}

static int32_t gemdos_ddelete(struct amtari_context *ctx)
{
    char path[AMTARI_PATH_MAX];
    int rc;
    if (ctx->fs.rmdir == 0) return AMTARI_EIO;
    rc = path_arg(ctx, 0, path, sizeof(path));
    return rc != 0 ? rc : amtari_gemdos_error_from_host(ctx->fs.rmdir(ctx->fs.opaque, path));
}

static int32_t gemdos_dsetpath(struct amtari_context *ctx)
{
    uint32_t address;
    char translated[AMTARI_PATH_MAX];
    const char *relative;
    uint8_t drive;
    int rc;
    if (read_arg32(ctx, 0, &address) != 0) return AMTARI_EFAULT;
    rc = amtari_path_translate(ctx, address, translated, sizeof(translated));
    if (rc != 0) return rc;
    drive = ctx->current_drive;
    if (translated[0] >= 'A' && translated[0] <= 'Z' && translated[1] == ':') drive = (uint8_t)(translated[0] - 'A');
    relative = translated + 3;
    if (strlen(relative) >= AMTARI_PATH_MAX) return AMTARI_EINVAL;
    strcpy(ctx->cwd[drive], relative);
    return 0;
}

static int32_t gemdos_dgetpath(struct amtari_context *ctx)
{
    uint32_t buffer;
    uint16_t drive_arg;
    uint8_t drive;
    size_t length;
    if (read_arg32(ctx, 0, &buffer) != 0 || read_arg16(ctx, 4, &drive_arg) != 0) return AMTARI_EFAULT;
    if (drive_arg == 0u) drive = ctx->current_drive;
    else if (drive_arg <= 26u) drive = (uint8_t)(drive_arg - 1u);
    else return AMTARI_ENOENT;
    if ((ctx->drive_mask & (1u << drive)) == 0u) return AMTARI_ENOENT;
    length = strlen(ctx->cwd[drive]) + 1u;
    if (!amtari_guest_range_valid(ctx, buffer, length)) return AMTARI_EFAULT;
    memcpy(&ctx->memory.data[buffer], ctx->cwd[drive], length);
    return 0;
}

static int32_t gemdos_fcreate(struct amtari_context *ctx)
{
    uint16_t attr;
    char path[AMTARI_PATH_MAX];
    int rc;
    if (ctx->fs.create == 0) return AMTARI_EIO;
    if (read_arg16(ctx, 4, &attr) != 0) return AMTARI_EFAULT;
    rc = path_arg(ctx, 0, path, sizeof(path));
    return rc != 0 ? rc : amtari_gemdos_error_from_host(ctx->fs.create(ctx->fs.opaque, path, attr));
}

static int32_t gemdos_fopen(struct amtari_context *ctx)
{
    uint32_t filename;
    uint16_t mode;
    char path[AMTARI_PATH_MAX];
    int rc;
    if (ctx->fs.open == 0) return AMTARI_EIO;
    if (read_arg32(ctx, 0, &filename) != 0 || read_arg16(ctx, 4, &mode) != 0) return AMTARI_EFAULT;
    rc = amtari_path_translate(ctx, filename, path, sizeof(path));
    return rc != 0 ? rc : amtari_gemdos_error_from_host(ctx->fs.open(ctx->fs.opaque, path, mode));
}

static int32_t gemdos_fclose(struct amtari_context *ctx)
{
    uint16_t handle;
    if (ctx->fs.close == 0) return AMTARI_EIO;
    if (read_arg16(ctx, 0, &handle) != 0) return AMTARI_EFAULT;
    return amtari_gemdos_error_from_host(ctx->fs.close(ctx->fs.opaque, (int16_t)handle));
}

static int32_t gemdos_fread(struct amtari_context *ctx)
{
    uint16_t handle;
    uint32_t count, buffer;
    if (ctx->fs.read == 0) return AMTARI_EIO;
    if (read_arg16(ctx, 0, &handle) != 0 || read_arg32(ctx, 2, &count) != 0 || read_arg32(ctx, 6, &buffer) != 0) return AMTARI_EFAULT;
    if (!amtari_guest_range_valid(ctx, buffer, (size_t)count)) return AMTARI_EFAULT;
    return amtari_gemdos_error_from_host(ctx->fs.read(ctx->fs.opaque, (int16_t)handle, &ctx->memory.data[buffer], count));
}

static int32_t gemdos_fwrite(struct amtari_context *ctx)
{
    uint16_t handle;
    uint32_t count, buffer;
    if (ctx->fs.write == 0) return AMTARI_EIO;
    if (read_arg16(ctx, 0, &handle) != 0 || read_arg32(ctx, 2, &count) != 0 || read_arg32(ctx, 6, &buffer) != 0) return AMTARI_EFAULT;
    if (!amtari_guest_range_valid(ctx, buffer, (size_t)count)) return AMTARI_EFAULT;
    return amtari_gemdos_error_from_host(ctx->fs.write(ctx->fs.opaque, (int16_t)handle, &ctx->memory.data[buffer], count));
}

static int32_t gemdos_fdelete(struct amtari_context *ctx)
{
    char path[AMTARI_PATH_MAX];
    int rc;
    if (ctx->fs.unlink == 0) return AMTARI_EIO;
    rc = path_arg(ctx, 0, path, sizeof(path));
    return rc != 0 ? rc : amtari_gemdos_error_from_host(ctx->fs.unlink(ctx->fs.opaque, path));
}

static int32_t gemdos_fseek(struct amtari_context *ctx)
{
    uint32_t raw_offset;
    uint16_t handle, mode;
    if (ctx->fs.seek == 0) return AMTARI_EIO;
    if (read_arg32(ctx, 0, &raw_offset) != 0 || read_arg16(ctx, 4, &handle) != 0 || read_arg16(ctx, 6, &mode) != 0) return AMTARI_EFAULT;
    if (mode > 2u) return AMTARI_EINVAL;
    return amtari_gemdos_error_from_host(ctx->fs.seek(ctx->fs.opaque, (int16_t)handle, (int32_t)raw_offset, mode));
}

static int32_t gemdos_pexec(struct amtari_context *ctx)
{
    uint16_t mode;
    uint32_t name, arg2, env;
    char path[AMTARI_PATH_MAX];

    if (read_arg16(ctx, 0, &mode) != 0 || read_arg32(ctx, 2, &name) != 0 ||
        read_arg32(ctx, 6, &arg2) != 0 || read_arg32(ctx, 10, &env) != 0) {
        return AMTARI_EFAULT;
    }
    (void)env;

    if (mode == 3u) {
        const uint8_t *image = 0;
        const uint8_t *cmdline = 0;
        size_t image_size = 0u;
        int rc;

        if (ctx->process.fetch == 0) return AMTARI_EIO;
        rc = amtari_path_translate(ctx, name, path, sizeof(path));
        if (rc != 0) return rc;
        if (arg2 != 0u) {
            size_t length;
            if (!amtari_guest_range_valid(ctx, arg2, 1u)) return AMTARI_EFAULT;
            length = (size_t)ctx->memory.data[arg2] + 1u;
            if (!amtari_guest_range_valid(ctx, arg2, length)) return AMTARI_EFAULT;
            cmdline = &ctx->memory.data[arg2];
        }
        rc = ctx->process.fetch(ctx->process.opaque, path, &image, &image_size);
        if (rc != 0) return amtari_gemdos_error_from_host(rc);
        if (image == 0) return AMTARI_EIO;
        return amtari_prg_load(ctx, image, image_size, ctx->next_load_address, cmdline);
    }

    if (mode == 4u) {
        uint32_t tbase;
        if (arg2 == 0u || amtari_guest_read32(ctx, arg2 + 0x08u, &tbase) != 0) return AMTARI_EFAULT;
        ctx->current_basepage = arg2;
        ctx->cpu.pc = tbase;
        return 0;
    }

    return AMTARI_ENOSYS;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    switch (function) {
    case 0x01: return gemdos_cconin(ctx);
    case 0x02: return gemdos_cconout(ctx);
    case 0x09: return gemdos_cconws(ctx);
    case 0x0e: return gemdos_dsetdrv(ctx);
    case 0x19: return gemdos_dgetdrv(ctx);
    case 0x39: return gemdos_dcreate(ctx);
    case 0x3a: return gemdos_ddelete(ctx);
    case 0x3b: return gemdos_dsetpath(ctx);
    case 0x3c: return gemdos_fcreate(ctx);
    case 0x3d: return gemdos_fopen(ctx);
    case 0x3e: return gemdos_fclose(ctx);
    case 0x3f: return gemdos_fread(ctx);
    case 0x40: return gemdos_fwrite(ctx);
    case 0x41: return gemdos_fdelete(ctx);
    case 0x42: return gemdos_fseek(ctx);
    case 0x47: return gemdos_dgetpath(ctx);
    case 0x4b: return gemdos_pexec(ctx);
    default: return AMTARI_ENOSYS;
    }
}
