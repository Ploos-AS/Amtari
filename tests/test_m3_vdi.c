#include <assert.h>
#include <stdint.h>

#include "amtari.h"

struct vdi_fixture {
    uint16_t opcode;
    uint16_t intin_count;
    uint16_t ptsin_count;
    int calls;
};

static int32_t vdi_dispatch(void *opaque, uint16_t opcode,
                            const int16_t *intin, uint16_t intin_count,
                            const int16_t *ptsin, uint16_t ptsin_count,
                            int16_t *intout, uint16_t intout_capacity,
                            int16_t *ptsout, uint16_t ptsout_capacity)
{
    struct vdi_fixture *fixture = (struct vdi_fixture *)opaque;

    fixture->opcode = opcode;
    fixture->intin_count = intin_count;
    fixture->ptsin_count = ptsin_count;
    ++fixture->calls;

    assert(intin_count == 2u);
    assert(intin != 0 && intin[0] == 7 && intin[1] == 9);
    assert(ptsin_count == 2u);
    assert(ptsin != 0 && ptsin[0] == 10 && ptsin[1] == 20);
    assert(intout_capacity >= 1u && intout != 0);
    assert(ptsout_capacity >= 2u && ptsout != 0);

    intout[0] = 42;
    ptsout[0] = 320;
    ptsout[1] = 200;
    return 0;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct vdi_fixture fixture = {0};
    int16_t intin[2] = {7, 9};
    int16_t ptsin[2] = {10, 20};
    int16_t intout[1] = {0};
    int16_t ptsout[2] = {0, 0};

    assert(amtari_vdi_bind(&ctx, vdi_dispatch, &fixture) == AMTARI_EINVAL);
    assert(amtari_init(&ctx) == 0);
    assert(amtari_vdi_dispatch(&ctx, 1u, intin, 2u, ptsin, 2u,
                               intout, 1u, ptsout, 2u) == AMTARI_ENOSYS);
    assert(amtari_vdi_bind(&ctx, vdi_dispatch, &fixture) == 0);
    assert(amtari_vdi_dispatch(&ctx, 100u, intin, 2u, ptsin, 2u,
                               intout, 1u, ptsout, 2u) == 0);
    assert(fixture.calls == 1);
    assert(fixture.opcode == 100u);
    assert(fixture.intin_count == 2u);
    assert(fixture.ptsin_count == 2u);
    assert(intout[0] == 42);
    assert(ptsout[0] == 320 && ptsout[1] == 200);
    assert(amtari_vdi_bind(&ctx, 0, 0) == 0);
    assert(amtari_vdi_dispatch(&ctx, 100u, intin, 2u, ptsin, 2u,
                               intout, 1u, ptsout, 2u) == AMTARI_ENOSYS);
    return 0;
}
