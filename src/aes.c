#include "amtari.h"

int amtari_aes_bind(struct amtari_context *ctx, amtari_aes_dispatch_fn dispatch_fn, void *opaque)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->aes.dispatch = dispatch_fn;
    ctx->aes.opaque = opaque;
    return 0;
}

int32_t amtari_aes_dispatch(struct amtari_context *ctx, uint16_t opcode,
                            const int16_t *intin, uint16_t intin_count,
                            int16_t *intout, uint16_t intout_capacity, uint16_t *intout_count,
                            const uint32_t *addrin, uint16_t addrin_count,
                            uint32_t *addrout, uint16_t addrout_capacity, uint16_t *addrout_count)
{
    int32_t rc;
    if (ctx == 0 || !ctx->initialized || intout_count == 0 || addrout_count == 0)
        return AMTARI_EINVAL;
    *intout_count = 0u;
    *addrout_count = 0u;
    if (ctx->aes.dispatch == 0) return AMTARI_ENOSYS;
    rc = ctx->aes.dispatch(ctx->aes.opaque, opcode, intin, intin_count,
                           intout, intout_capacity, intout_count,
                           addrin, addrin_count, addrout, addrout_capacity, addrout_count);
    if (rc != 0) return rc;
    if (*intout_count > intout_capacity || *addrout_count > addrout_capacity)
        return AMTARI_EINVAL;
    return 0;
}
