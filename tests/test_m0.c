#include <assert.h>

#include "amtari.h"

int main(void)
{
    struct amtari_context ctx = {0};

    assert(amtari_version() != 0);
    assert(amtari_init(0) == -1);
    assert(amtari_init(&ctx) == 0);
    assert(ctx.initialized == 1);
    assert(ctx.machine == AMTARI_MACHINE_ST);
    assert(ctx.mode == AMTARI_MODE_NATIVE);

    return 0;
}
