#include "amtari.h"

static int bios_read_arg16(const struct amtari_context *ctx, uint32_t offset, uint16_t *value)
{
    return amtari_guest_read16(ctx, ctx->cpu.a[7] + 2u + offset, value);
}

static int32_t bios_bconstat(struct amtari_context *ctx)
{
    uint16_t dev;
    int ready;

    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.input_ready == 0) return AMTARI_EIO;

    ready = ctx->console.input_ready(ctx->console.opaque);
    if (ready < 0) return AMTARI_EIO;
    return ready ? -1 : 0;
}

static int32_t bios_bconin(struct amtari_context *ctx)
{
    uint16_t dev;
    int value;

    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.getc == 0) return AMTARI_EIO;

    value = ctx->console.getc(ctx->console.opaque);
    return value < 0 ? AMTARI_EIO : (int32_t)(value & 0xff);
}

static int32_t bios_bconout(struct amtari_context *ctx)
{
    uint16_t dev;
    uint16_t value;

    if (bios_read_arg16(ctx, 0u, &dev) != 0 ||
        bios_read_arg16(ctx, 2u, &value) != 0)
        return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.putc == 0) return AMTARI_EIO;

    return ctx->console.putc(ctx->console.opaque,
                             (unsigned char)(value & 0xffu)) < 0
               ? AMTARI_EIO
               : 0;
}

static int32_t bios_bcostat(struct amtari_context *ctx)
{
    uint16_t dev;
    int ready;

    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.output_ready == 0) return AMTARI_EIO;

    ready = ctx->console.output_ready(ctx->console.opaque);
    if (ready < 0) return AMTARI_EIO;
    return ready ? -1 : 0;
}

static int32_t dispatch_bios(struct amtari_context *ctx, uint16_t function)
{
    switch (function) {
    case 0x01u: /* Bconstat */
        return bios_bconstat(ctx);
    case 0x02u: /* Bconin */
        return bios_bconin(ctx);
    case 0x03u: /* Bconout */
        return bios_bconout(ctx);
    case 0x08u: /* Bcostat */
        return bios_bcostat(ctx);
    case 0x0au: /* Drvmap */
        return (int32_t)ctx->drive_mask;
    default:
        return AMTARI_ENOSYS;
    }
}

static int32_t xbios_random(struct amtari_context *ctx)
{
    ctx->random_seed = ctx->random_seed * UINT32_C(3141592621) + 1u;
    return (int32_t)((ctx->random_seed >> 8) & UINT32_C(0x00ffffff));
}

static int32_t dispatch_xbios(struct amtari_context *ctx, uint16_t function)
{
    switch (function) {
    case 0x11u: /* Random */
        return xbios_random(ctx);
    default:
        return AMTARI_ENOSYS;
    }
}

enum amtari_trap_kind amtari_trap_decode(unsigned int trap_number)
{
    switch (trap_number) {
    case 1:
        return AMTARI_TRAP_GEMDOS;
    case 13:
        return AMTARI_TRAP_BIOS;
    case 14:
        return AMTARI_TRAP_XBIOS;
    default:
        return AMTARI_TRAP_UNKNOWN;
    }
}

int32_t amtari_trap_dispatch(struct amtari_context *ctx, unsigned int trap_number, uint16_t function)
{
    if (ctx == 0 || !ctx->initialized) {
        return AMTARI_EINVAL;
    }

    switch (amtari_trap_decode(trap_number)) {
    case AMTARI_TRAP_GEMDOS:
        return amtari_gemdos_dispatch(ctx, function);
    case AMTARI_TRAP_BIOS:
        return dispatch_bios(ctx, function);
    case AMTARI_TRAP_XBIOS:
        return dispatch_xbios(ctx, function);
    case AMTARI_TRAP_UNKNOWN:
    default:
        return AMTARI_ENOSYS;
    }
}
