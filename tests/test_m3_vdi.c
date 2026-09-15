#include <assert.h>
#include <stdint.h>

#include "amtari.h"

struct vdi_fixture { uint16_t opcode; uint16_t intin_count; uint16_t ptsin_count; int calls; };
static int32_t vdi_dispatch(void *opaque, uint16_t opcode, const int16_t *intin, uint16_t intin_count, const int16_t *ptsin, uint16_t ptsin_count, int16_t *intout, uint16_t intout_capacity, uint16_t *intout_count, int16_t *ptsout, uint16_t ptsout_capacity, uint16_t *ptsout_count)
{
    struct vdi_fixture *f=(struct vdi_fixture *)opaque; f->opcode=opcode; f->intin_count=intin_count; f->ptsin_count=ptsin_count; ++f->calls;
    assert(intin_count==2u && intin && intin[0]==7 && intin[1]==9); assert(ptsin_count==2u && ptsin && ptsin[0]==10 && ptsin[1]==20);
    assert(intout_capacity>=1u && intout); assert(ptsout_capacity>=2u && ptsout); intout[0]=42; ptsout[0]=320; ptsout[1]=200; *intout_count=1u; *ptsout_count=2u; return 0;
}
static void put16(uint8_t *m,uint32_t a,uint16_t v){m[a]=(uint8_t)(v>>8);m[a+1u]=(uint8_t)v;}
static void put32(uint8_t *m,uint32_t a,uint32_t v){m[a]=(uint8_t)(v>>24);m[a+1u]=(uint8_t)(v>>16);m[a+2u]=(uint8_t)(v>>8);m[a+3u]=(uint8_t)v;}
static uint16_t get16(const uint8_t *m,uint32_t a){return (uint16_t)(((uint16_t)m[a]<<8)|m[a+1u]);}
int main(void)
{
    struct amtari_context ctx={0}; struct vdi_fixture f={0}; int16_t intin[2]={7,9},ptsin[2]={10,20},intout[8]={0},ptsout[8]={0}; uint16_t ni=0u,np=0u; uint8_t m[512]={0};
    assert(amtari_vdi_bind(&ctx,vdi_dispatch,&f)==AMTARI_EINVAL); assert(amtari_init(&ctx)==0);
    assert(amtari_vdi_dispatch(&ctx,1u,intin,2u,ptsin,2u,intout,8u,&ni,ptsout,8u,&np)==AMTARI_ENOSYS);
    assert(amtari_vdi_configure(&ctx,640u,400u,16u)==0);
    assert(amtari_vdi_dispatch(&ctx,1u,0,0u,0,0u,intout,8u,&ni,ptsout,8u,&np)==0);
    assert(ni==3u && np==2u && intout[0]==1 && intout[1]==16 && intout[2]==1 && ptsout[0]==639 && ptsout[1]==399);
    assert(amtari_vdi_bind(&ctx,vdi_dispatch,&f)==0); assert(amtari_vdi_dispatch(&ctx,100u,intin,2u,ptsin,2u,intout,8u,&ni,ptsout,8u,&np)==0);
    assert(f.calls==1 && f.opcode==100u && ni==1u && np==2u && intout[0]==42 && ptsout[0]==320 && ptsout[1]==200);

    assert(amtari_guest_memory_bind(&ctx,m,sizeof(m))==0);
    put32(m,0x20u,0x40u); put32(m,0x24u,0x80u); put32(m,0x28u,0x90u); put32(m,0x2cu,0xa0u); put32(m,0x30u,0xb0u);
    put16(m,0x40u,100u); put16(m,0x42u,1u); put16(m,0x44u,0u); put16(m,0x46u,2u); put16(m,0x48u,0u); put16(m,0x80u,7u); put16(m,0x82u,9u); put16(m,0x90u,10u); put16(m,0x92u,20u);
    ctx.cpu.d[1]=0x20u; assert(amtari_trap_dispatch(&ctx,2u,0x73u)==0); assert(f.calls==2 && get16(m,0x44u)==1u && get16(m,0x48u)==1u); assert(get16(m,0xa0u)==42u); assert(get16(m,0xb0u)==320u && get16(m,0xb2u)==200u);
    assert(amtari_trap_dispatch(&ctx,2u,0xc8u)==AMTARI_ENOSYS);
    return 0;
}
