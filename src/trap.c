#include "amtari.h"

static int32_t dispatch_bios(struct amtari_context *ctx, uint16_t function)
{
    (void)ctx;
    (void)function;
    return AMTARI_ENOSYS;
}

static int32_t dispatch_xbios(struct amtari_context *ctx, uint16_t function)
{
    (void)ctx;
    (void)function;
    return AMTARI_ENOSYS;
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
