#include <assert.h>
#include <stdint.h>
#include "amtari.h"

struct fixture { int calls; uint16_t opcode; };

static int32_t aes_backend(void *opaque, uint16_t opcode,
                           const int16_t *intin, uint16_t intin_count,
                           int16_t *intout, uint16_t intout_capacity, uint16_t *intout_count,
                           const uint32_t *addrin, uint16_t addrin_count,
                           uint32_t *addrout, uint16_t addrout_capacity, uint16_t *addrout_count)
{
    struct fixture *f = (struct fixture *)opaque;
    assert(intin_count == 2u && intin[0] == 7 && intin[1] == 9);
    assert(addrin_count == 1u && addrin[0] == UINT32_C(0x12345678));
    assert(intout_capacity >= 1u && addrout_capacity >= 1u);
    ++f->calls; f->opcode = opcode;
    intout[0] = 16; *intout_count = 1u;
    addrout[0] = UINT32_C(0x87654321); *addrout_count = 1u;
    return 0;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct fixture f = {0};
    int16_t intin[2] = {7,9}, intout[4] = {0};
    uint32_t addrin[1] = {UINT32_C(0x12345678)}, addrout[2] = {0};
    uint16_t ni = 99u, na = 99u;
    assert(amtari_init(&ctx) == 0);
    assert(amtari_aes_dispatch(&ctx,10u,intin,2u,intout,4u,&ni,addrin,1u,addrout,2u,&na) == AMTARI_ENOSYS);
    assert(ni == 0u && na == 0u);
    assert(amtari_aes_bind(&ctx,aes_backend,&f) == 0);
    assert(amtari_aes_dispatch(&ctx,10u,intin,2u,intout,4u,&ni,addrin,1u,addrout,2u,&na) == 0);
    assert(f.calls == 1 && f.opcode == 10u);
    assert(ni == 1u && intout[0] == 16);
    assert(na == 1u && addrout[0] == UINT32_C(0x87654321));
    return 0;
}
