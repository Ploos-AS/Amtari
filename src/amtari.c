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
    ctx->next_load_address = 0x1000u;
    ctx->random_seed = 1u;
    ctx->aes.workstation_handle = 1u;
    ctx->aes.next_window_handle = 1u;
    ctx->aes.char_width = 8u;
    ctx->aes.char_height = 16u;
    ctx->aes.box_width = 8u;
    ctx->aes.box_height = 16u;
    ctx->initialized = 1;
    return 0;
}

int amtari_random_seed(struct amtari_context *ctx, uint32_t seed)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->random_seed = seed;
    return 0;
}

int amtari_clock_bind(struct amtari_context *ctx, amtari_clock_get_fn get_fn, void *opaque)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->clock.get = get_fn;
    ctx->clock.set = 0;
    ctx->clock.opaque = opaque;
    return 0;
}

int amtari_clock_bind_rw(struct amtari_context *ctx, amtari_clock_get_fn get_fn,
                         amtari_clock_set_fn set_fn, void *opaque)
{
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    ctx->clock.get = get_fn;
    ctx->clock.set = set_fn;
    ctx->clock.opaque = opaque;
    return 0;
}

int amtari_console_status_bind(struct amtari_context *ctx,
                               amtari_console_status_fn input_ready_fn,
                               amtari_console_status_fn output_ready_fn)
{
    if (ctx == 0) return AMTARI_EINVAL;
    ctx->console.input_ready = input_ready_fn;
    ctx->console.output_ready = output_ready_fn;
    return 0;
}
