/* M2.14 GEMDOS process-termination extension.
 *
 * Keep the established GEMDOS implementation intact and add Pterm0/Pterm.
 * The execution core currently halts on the following RTS sentinel, so these
 * handlers consume their GEMDOS stack arguments before returning control.
 */
#define amtari_gemdos_dispatch amtari_gemdos_dispatch_m213
#include "gemdos.c"
#undef amtari_gemdos_dispatch

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    uint16_t code;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;

    if (function == 0x00u) { /* Pterm0 */
        ctx->cpu.a[7] += 2u; /* discard GEMDOS function word */
        return 0;
    }

    if (function == 0x4cu) { /* Pterm(short returncode) */
        if (amtari_guest_read16(ctx, ctx->cpu.a[7] + 2u, &code) != 0)
            return AMTARI_EFAULT;
        ctx->cpu.a[7] += 4u; /* function word + return code */
        return (int32_t)(int16_t)code;
    }

    return amtari_gemdos_dispatch_m213(ctx, function);
}
