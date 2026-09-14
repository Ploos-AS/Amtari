#include "amtari.h"

const char *amtari_version(void)
{
    return AMTARI_VERSION;
}

int amtari_init(struct amtari_context *ctx)
{
    if (ctx == 0) {
        return AMTARI_EINVAL;
    }

    ctx->machine = AMTARI_MACHINE_ST;
    ctx->mode = AMTARI_MODE_NATIVE;
    ctx->drive_mask = (1u << 2); /* C: by default; host may replace this map. */
    ctx->current_drive = 2;
    ctx->initialized = 1;
    return 0;
}
