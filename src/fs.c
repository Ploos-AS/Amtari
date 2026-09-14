#include <ctype.h>

#include "amtari.h"

int amtari_fs_bind(struct amtari_context *ctx, amtari_fs_open_fn open_fn,
                   amtari_fs_close_fn close_fn, amtari_fs_read_fn read_fn,
                   amtari_fs_write_fn write_fn, void *opaque)
{
    if (ctx == 0) {
        return AMTARI_EINVAL;
    }

    ctx->fs.open = open_fn;
    ctx->fs.close = close_fn;
    ctx->fs.read = read_fn;
    ctx->fs.write = write_fn;
    ctx->fs.opaque = opaque;
    return 0;
}

int amtari_fs_set_drives(struct amtari_context *ctx, uint32_t drive_mask, uint8_t current_drive)
{
    if (ctx == 0 || current_drive >= 26u || (drive_mask & (1u << current_drive)) == 0u) {
        return AMTARI_EINVAL;
    }

    ctx->drive_mask = drive_mask & 0x03ffffffu;
    ctx->current_drive = current_drive;
    return 0;
}

int amtari_path_translate(const struct amtari_context *ctx, uint32_t guest_address,
                          char *output, size_t output_size)
{
    size_t in_pos = 0;
    size_t out_pos = 0;
    uint8_t drive;
    int explicit_drive = 0;

    if (ctx == 0 || output == 0 || output_size < 4u) {
        return AMTARI_EINVAL;
    }
    if (!amtari_guest_range_valid(ctx, guest_address, 1)) {
        return AMTARI_EFAULT;
    }

    drive = ctx->current_drive;
    if (amtari_guest_range_valid(ctx, guest_address, 2) &&
        isalpha((unsigned char)ctx->memory.data[guest_address]) &&
        ctx->memory.data[guest_address + 1u] == ':') {
        unsigned char letter = (unsigned char)toupper((unsigned char)ctx->memory.data[guest_address]);
        drive = (uint8_t)(letter - 'A');
        explicit_drive = 1;
        in_pos = 2;
    }

    if (drive >= 26u || (ctx->drive_mask & (1u << drive)) == 0u) {
        return AMTARI_ENOENT;
    }

    output[out_pos++] = (char)('A' + drive);
    output[out_pos++] = ':';
    output[out_pos++] = '/';

    while (out_pos + 1u < output_size) {
        unsigned char ch;
        uint32_t address = guest_address + (uint32_t)in_pos;

        if (!amtari_guest_range_valid(ctx, address, 1)) {
            return AMTARI_EFAULT;
        }
        ch = ctx->memory.data[address];
        if (ch == 0u) {
            output[out_pos] = '\0';
            return 0;
        }
        ++in_pos;

        if (explicit_drive && in_pos == 3u && (ch == '\\' || ch == '/')) {
            continue;
        }
        if (!explicit_drive && in_pos == 1u && (ch == '\\' || ch == '/')) {
            continue;
        }

        output[out_pos++] = (ch == '\\') ? '/' : (char)ch;
    }

    output[output_size - 1u] = '\0';
    return AMTARI_EINVAL;
}
