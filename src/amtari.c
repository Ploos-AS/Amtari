#include <string.h>

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

    memset(ctx, 0, sizeof(*ctx));
    ctx->machine = AMTARI_MACHINE_ST;
    ctx->mode = AMTARI_MODE_NATIVE;
    ctx->drive_mask = 1u;
    ctx->current_drive = 0u;
    ctx->initialized = 1;
    return 0;
}
