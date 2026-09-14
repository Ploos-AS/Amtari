#include "amtari.h"

const char *amtari_version(void)
{
    return AMTARI_VERSION;
}

int amtari_init(struct amtari_context *ctx)
{
    if (ctx == 0) {
        return -1;
    }

    ctx->machine = AMTARI_MACHINE_ST;
    ctx->mode = AMTARI_MODE_NATIVE;
    ctx->initialized = 1;
    return 0;
}
