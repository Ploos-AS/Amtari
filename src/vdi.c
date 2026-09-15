#include "amtari.h"

int amtari_vdi_bind(struct amtari_context *ctx, amtari_vdi_dispatch_fn dispatch_fn,
                    void *opaque)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->vdi.dispatch = dispatch_fn;
    ctx->vdi.opaque = opaque;
    return 0;
}

int amtari_vdi_configure(struct amtari_context *ctx, uint16_t width, uint16_t height,
                         uint16_t colors)
{
    if (ctx == 0 || !ctx->initialized || width == 0u || height == 0u || colors == 0u)
        return AMTARI_EINVAL;
    ctx->vdi.width = width;
    ctx->vdi.height = height;
    ctx->vdi.colors = colors;
    if (ctx->vdi.next_handle == 0u) ctx->vdi.next_handle = 1u;
    return 0;
}

static int32_t vdi_open_workstation(struct amtari_context *ctx,
                                    int16_t *intout, uint16_t intout_capacity,
                                    uint16_t *intout_count,
                                    int16_t *ptsout, uint16_t ptsout_capacity,
                                    uint16_t *ptsout_count)
{
    if (ctx->vdi.width == 0u || ctx->vdi.height == 0u || ctx->vdi.colors == 0u)
        return AMTARI_ENOSYS;
    if (intout_capacity < 3u || ptsout_capacity < 2u || intout == 0 || ptsout == 0)
        return AMTARI_EINVAL;
    intout[0] = (int16_t)ctx->vdi.next_handle++;
    intout[1] = (int16_t)ctx->vdi.colors;
    intout[2] = 1; /* raster coordinate device */
    ptsout[0] = (int16_t)(ctx->vdi.width - 1u);
    ptsout[1] = (int16_t)(ctx->vdi.height - 1u);
    *intout_count = 3u;
    *ptsout_count = 2u;
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
    *intout_count = 0u;
    *ptsout_count = 0u;

    /* M3.3 builtin subset: v_opnwk (opcode 1). Backend handles everything else. */
    if (opcode == 1u)
        return vdi_open_workstation(ctx, intout, intout_capacity, intout_count,
                                    ptsout, ptsout_capacity, ptsout_count);
    if (ctx->vdi.dispatch == 0) return AMTARI_ENOSYS;

    rc = ctx->vdi.dispatch(ctx->vdi.opaque, opcode, intin, intin_count, ptsin, ptsin_count,
                           intout, intout_capacity, intout_count,
                           ptsout, ptsout_capacity, ptsout_count);
    if (rc != 0) return rc;
    if (*intout_count > intout_capacity || *ptsout_count > ptsout_capacity)
        return AMTARI_EINVAL;
    return 0;
}
