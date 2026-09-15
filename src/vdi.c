#include "amtari.h"

int amtari_vdi_bind(struct amtari_context *ctx, amtari_vdi_dispatch_fn dispatch_fn,
                    void *opaque)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->vdi.dispatch = dispatch_fn;
    ctx->vdi.opaque = opaque;
    return 0;
}

int32_t amtari_vdi_dispatch(struct amtari_context *ctx, uint16_t opcode,
                            const int16_t *intin, uint16_t intin_count,
                            const int16_t *ptsin, uint16_t ptsin_count,
                            int16_t *intout, uint16_t intout_capacity,
                            int16_t *ptsout, uint16_t ptsout_capacity)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    if (ctx->vdi.dispatch == 0) return AMTARI_ENOSYS;

    return ctx->vdi.dispatch(ctx->vdi.opaque, opcode,
                             intin, intin_count,
                             ptsin, ptsin_count,
                             intout, intout_capacity,
                             ptsout, ptsout_capacity);
}
