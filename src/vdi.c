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
                            uint16_t *intout_count,
                            int16_t *ptsout, uint16_t ptsout_capacity,
                            uint16_t *ptsout_count)
{
    int32_t rc;

    if (ctx == 0 || !ctx->initialized || intout_count == 0 || ptsout_count == 0)
        return AMTARI_EINVAL;
    if (ctx->vdi.dispatch == 0) return AMTARI_ENOSYS;

    *intout_count = 0u;
    *ptsout_count = 0u;
    rc = ctx->vdi.dispatch(ctx->vdi.opaque, opcode,
                           intin, intin_count,
                           ptsin, ptsin_count,
                           intout, intout_capacity, intout_count,
                           ptsout, ptsout_capacity, ptsout_count);
    if (rc != 0) return rc;
    if (*intout_count > intout_capacity || *ptsout_count > ptsout_capacity)
        return AMTARI_EINVAL;
    return 0;
}
