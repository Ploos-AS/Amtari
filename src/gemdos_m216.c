/* M2.16 GEMDOS process extension: Pexec(0) load-and-go.
 *
 * Execute one child process synchronously, preserve the parent CPU/basepage,
 * and return the child's exit code to the parent.  This intentionally starts
 * with one child depth; nested Pexec can be added after the basic lifecycle is
 * proven stable.
 */
#define AMTARI_M214_DISPATCH_NAME amtari_gemdos_dispatch_m214
#include "gemdos_m214.c"
#undef AMTARI_M214_DISPATCH_NAME

static int32_t m216_pexec_load_and_go(struct amtari_context *ctx)
{
    uint16_t mode;
    uint32_t name, cmdline_address, env;
    const uint8_t *image = 0;
    const uint8_t *cmdline = 0;
    size_t image_size = 0u;
    char path[AMTARI_PATH_MAX];
    struct amtari_cpu_state parent_cpu;
    uint32_t parent_basepage;
    uint32_t basepage;
    uint32_t hitpa;
    uint32_t steps = 0u;
    int32_t load_result;
    int child_rc;
    int rc;

    if (read_arg16(ctx, 0u, &mode) != 0 || read_arg32(ctx, 2u, &name) != 0 ||
        read_arg32(ctx, 6u, &cmdline_address) != 0 || read_arg32(ctx, 10u, &env) != 0)
        return AMTARI_EFAULT;
    (void)env;
    if (mode != 0u) return AMTARI_ENOSYS;
    if (ctx->process_depth != 0u) return AMTARI_ENOSYS;
    if (ctx->process.fetch == 0) return AMTARI_EIO;

    rc = amtari_path_translate(ctx, name, path, sizeof(path));
    if (rc != 0) return rc;
    if (cmdline_address != 0u) {
        size_t length;
        if (!amtari_guest_range_valid(ctx, cmdline_address, 1u)) return AMTARI_EFAULT;
        length = (size_t)ctx->memory.data[cmdline_address] + 1u;
        if (!amtari_guest_range_valid(ctx, cmdline_address, length)) return AMTARI_EFAULT;
        cmdline = &ctx->memory.data[cmdline_address];
    }

    rc = ctx->process.fetch(ctx->process.opaque, path, &image, &image_size);
    if (rc != 0) return amtari_gemdos_error_from_host(rc);
    if (image == 0) return AMTARI_EIO;

    parent_cpu = ctx->cpu;
    parent_basepage = ctx->current_basepage;
    basepage = ctx->next_load_address;
    load_result = amtari_prg_load(ctx, image, image_size, basepage, cmdline);
    if (load_result < 0) return load_result;
    if (amtari_guest_read32(ctx, basepage + 0x04u, &hitpa) != 0) return AMTARI_EFAULT;

    ctx->process_depth = 1u;
    rc = amtari_exec_prepare(ctx, basepage, hitpa);
    if (rc == 0) child_rc = amtari_exec_run(ctx, 65536u, &steps);
    else child_rc = rc;

    if (child_rc == AMTARI_EXEC_HALTED) child_rc = (int32_t)ctx->cpu.d[0];
    ctx->cpu = parent_cpu;
    ctx->current_basepage = parent_basepage;
    ctx->process_depth = 0u;

    return child_rc;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    uint16_t mode;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    if (function != 0x4bu) return amtari_gemdos_dispatch_m214(ctx, function);
    if (read_arg16(ctx, 0u, &mode) != 0) return AMTARI_EFAULT;
    if (mode != 0u) return amtari_gemdos_dispatch_m214(ctx, function);
    return m216_pexec_load_and_go(ctx);
}
