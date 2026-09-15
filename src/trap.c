#include "amtari.h"

#define AMTARI_VDI_WORD_MAX 256u

static int bios_read_arg16(const struct amtari_context *ctx, uint32_t offset, uint16_t *value)
{
    return amtari_guest_read16(ctx, ctx->cpu.a[7] + 2u + offset, value);
}

static int xbios_read_arg32(const struct amtari_context *ctx, uint32_t offset, uint32_t *value)
{
    return amtari_guest_read32(ctx, ctx->cpu.a[7] + 2u + offset, value);
}

static int32_t dispatch_vdi_trap(struct amtari_context *ctx)
{
    uint32_t pb = ctx->cpu.d[1];
    uint32_t contrl, intin_addr, ptsin_addr, intout_addr, ptsout_addr;
    uint16_t opcode, ptsin_pairs, intin_count;
    uint16_t ptsin_count, intout_count = 0u, ptsout_count = 0u;
    int16_t intin[AMTARI_VDI_WORD_MAX];
    int16_t ptsin[AMTARI_VDI_WORD_MAX];
    int16_t intout[AMTARI_VDI_WORD_MAX];
    int16_t ptsout[AMTARI_VDI_WORD_MAX];
    uint16_t value;
    uint16_t i;
    int32_t rc;

    if (amtari_guest_read32(ctx, pb + 0u, &contrl) != 0 ||
        amtari_guest_read32(ctx, pb + 4u, &intin_addr) != 0 ||
        amtari_guest_read32(ctx, pb + 8u, &ptsin_addr) != 0 ||
        amtari_guest_read32(ctx, pb + 12u, &intout_addr) != 0 ||
        amtari_guest_read32(ctx, pb + 16u, &ptsout_addr) != 0)
        return AMTARI_EFAULT;

    if (amtari_guest_read16(ctx, contrl + 0u, &opcode) != 0 ||
        amtari_guest_read16(ctx, contrl + 2u, &ptsin_pairs) != 0 ||
        amtari_guest_read16(ctx, contrl + 6u, &intin_count) != 0)
        return AMTARI_EFAULT;

    if (ptsin_pairs > AMTARI_VDI_WORD_MAX / 2u || intin_count > AMTARI_VDI_WORD_MAX)
        return AMTARI_EINVAL;
    ptsin_count = (uint16_t)(ptsin_pairs * 2u);

    for (i = 0u; i < intin_count; ++i) {
        if (amtari_guest_read16(ctx, intin_addr + (uint32_t)i * 2u, &value) != 0)
            return AMTARI_EFAULT;
        intin[i] = (int16_t)value;
    }
    for (i = 0u; i < ptsin_count; ++i) {
        if (amtari_guest_read16(ctx, ptsin_addr + (uint32_t)i * 2u, &value) != 0)
            return AMTARI_EFAULT;
        ptsin[i] = (int16_t)value;
    }

    rc = amtari_vdi_dispatch(ctx, opcode,
                             intin_count ? intin : 0, intin_count,
                             ptsin_count ? ptsin : 0, ptsin_count,
                             intout, AMTARI_VDI_WORD_MAX, &intout_count,
                             ptsout, AMTARI_VDI_WORD_MAX, &ptsout_count);
    if (rc != 0) return rc;
    if ((ptsout_count & 1u) != 0u) return AMTARI_EINVAL;

    for (i = 0u; i < intout_count; ++i) {
        if (amtari_guest_write16(ctx, intout_addr + (uint32_t)i * 2u,
                                 (uint16_t)intout[i]) != 0)
            return AMTARI_EFAULT;
    }
    for (i = 0u; i < ptsout_count; ++i) {
        if (amtari_guest_write16(ctx, ptsout_addr + (uint32_t)i * 2u,
                                 (uint16_t)ptsout[i]) != 0)
            return AMTARI_EFAULT;
    }
    if (amtari_guest_write16(ctx, contrl + 4u, (uint16_t)(ptsout_count / 2u)) != 0 ||
        amtari_guest_write16(ctx, contrl + 8u, intout_count) != 0)
        return AMTARI_EFAULT;

    /* Atari VDI returns the workstation handle in contrl[6].  The builtin
     * v_opnwk dispatcher keeps its compact host API result in intout[0];
     * translate that result to the historical guest ABI here. */
    if (opcode == 1u) {
        if (intout_count == 0u) return AMTARI_EIO;
        if (amtari_guest_write16(ctx, contrl + 12u, (uint16_t)intout[0]) != 0)
            return AMTARI_EFAULT;
    }
    return 0;
}

static int32_t dispatch_gem(struct amtari_context *ctx, uint16_t selector)
{
    if (selector == 0x0073u) return dispatch_vdi_trap(ctx);
    return AMTARI_ENOSYS;
}

static int32_t bios_bconstat(struct amtari_context *ctx)
{
    uint16_t dev; int ready;
    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.input_ready == 0) return AMTARI_EIO;
    ready = ctx->console.input_ready(ctx->console.opaque);
    if (ready < 0) return AMTARI_EIO;
    return ready ? -1 : 0;
}
static int32_t bios_bconin(struct amtari_context *ctx)
{
    uint16_t dev; int value;
    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.getc == 0) return AMTARI_EIO;
    value = ctx->console.getc(ctx->console.opaque);
    return value < 0 ? AMTARI_EIO : (int32_t)(value & 0xff);
}
static int32_t bios_bconout(struct amtari_context *ctx)
{
    uint16_t dev, value;
    if (bios_read_arg16(ctx, 0u, &dev) != 0 || bios_read_arg16(ctx, 2u, &value) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.putc == 0) return AMTARI_EIO;
    return ctx->console.putc(ctx->console.opaque, (unsigned char)(value & 0xffu)) < 0 ? AMTARI_EIO : 0;
}
static int32_t bios_bcostat(struct amtari_context *ctx)
{
    uint16_t dev; int ready;
    if (bios_read_arg16(ctx, 0u, &dev) != 0) return AMTARI_EFAULT;
    if (dev != 2u) return AMTARI_ENOSYS;
    if (ctx->console.output_ready == 0) return AMTARI_EIO;
    ready = ctx->console.output_ready(ctx->console.opaque);
    if (ready < 0) return AMTARI_EIO;
    return ready ? -1 : 0;
}
static int32_t dispatch_bios(struct amtari_context *ctx, uint16_t function)
{
    switch (function) { case 0x01u:return bios_bconstat(ctx); case 0x02u:return bios_bconin(ctx); case 0x03u:return bios_bconout(ctx); case 0x08u:return bios_bcostat(ctx); case 0x0au:return (int32_t)ctx->drive_mask; default:return AMTARI_ENOSYS; }
}
static int32_t xbios_random(struct amtari_context *ctx)
{ ctx->random_seed=ctx->random_seed*UINT32_C(3141592621)+1u; return (int32_t)((ctx->random_seed>>8)&UINT32_C(0x00ffffff)); }
static int32_t xbios_settime(struct amtari_context *ctx)
{ uint32_t t;if(xbios_read_arg32(ctx,0u,&t)!=0)return AMTARI_EFAULT;if(ctx->clock.set==0)return AMTARI_EIO;return ctx->clock.set(ctx->clock.opaque,t)==0?0:AMTARI_EIO; }
static int32_t xbios_gettime(struct amtari_context *ctx)
{ uint32_t t;if(ctx->clock.get==0)return AMTARI_EIO;if(ctx->clock.get(ctx->clock.opaque,&t)!=0)return AMTARI_EIO;return (int32_t)t; }
static int32_t dispatch_xbios(struct amtari_context *ctx,uint16_t function)
{ switch(function){case 0x11u:return xbios_random(ctx);case 0x16u:return xbios_settime(ctx);case 0x17u:return xbios_gettime(ctx);default:return AMTARI_ENOSYS;} }
enum amtari_trap_kind amtari_trap_decode(unsigned int trap_number)
{ switch(trap_number){case 1:return AMTARI_TRAP_GEMDOS;case 2:return AMTARI_TRAP_GEM;case 13:return AMTARI_TRAP_BIOS;case 14:return AMTARI_TRAP_XBIOS;default:return AMTARI_TRAP_UNKNOWN;} }
int32_t amtari_trap_dispatch(struct amtari_context *ctx,unsigned int trap_number,uint16_t function)
{ if(ctx==0||!ctx->initialized)return AMTARI_EINVAL;switch(amtari_trap_decode(trap_number)){case AMTARI_TRAP_GEMDOS:return amtari_gemdos_dispatch(ctx,function);case AMTARI_TRAP_GEM:return dispatch_gem(ctx,function);case AMTARI_TRAP_BIOS:return dispatch_bios(ctx,function);case AMTARI_TRAP_XBIOS:return dispatch_xbios(ctx,function);case AMTARI_TRAP_UNKNOWN:default:return AMTARI_ENOSYS;} }
