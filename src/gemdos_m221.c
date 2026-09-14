/* M2.21 GEMDOS process ownership extension.
 *
 * M2.20 provides the fragmented/coalescing heap and Pexec isolation. M2.21
 * adds an explicit process ownership boundary for mutating memory operations:
 * a process may only Mfree/Mshrink blocks allocated by its own basepage.
 * Child-owned blocks are still reclaimed automatically when synchronous
 * Pexec(0) returns via the M2.20 lifecycle path.
 */
#define amtari_gemdos_dispatch amtari_gemdos_dispatch_m220
#include "gemdos_m216.c"
#undef amtari_gemdos_dispatch

static int m221_lookup_block(const struct amtari_context *ctx, uint32_t address)
{
    unsigned int i;
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        if (ctx->mem_blocks[i].valid && ctx->mem_blocks[i].in_use &&
            ctx->mem_blocks[i].address == address) return (int)i;
    }
    return -1;
}

static int32_t m221_check_owner(struct amtari_context *ctx, uint32_t arg_offset)
{
    uint32_t address;
    int slot;

    if (read_arg32(ctx, arg_offset, &address) != 0) return AMTARI_EFAULT;
    slot = m221_lookup_block(ctx, address);
    if (slot < 0) return AMTARI_EINVAL;
    if (ctx->mem_blocks[slot].owner_basepage != ctx->current_basepage)
        return AMTARI_EACCES;
    return 0;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    int32_t rc;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;

    if (function == 0x49u) {
        rc = m221_check_owner(ctx, 0u);
        if (rc != 0) return rc;
    } else if (function == 0x4au) {
        rc = m221_check_owner(ctx, 2u);
        if (rc != 0) return rc;
    }

    return amtari_gemdos_dispatch_m220(ctx, function);
}
