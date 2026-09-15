#include <assert.h>
#include <stdint.h>

#include "amtari.h"

struct console_fixture { int input; int input_ready; int output_ready; int output_count; unsigned char output; };
struct clock_fixture { uint32_t value; uint32_t set_value; int fail; int set_fail; int set_count; };

static int console_getc(void *opaque) { return ((struct console_fixture *)opaque)->input; }
static int console_putc(void *opaque, unsigned char ch) { struct console_fixture *f = opaque; f->output = ch; ++f->output_count; return 0; }
static int console_input_ready(void *opaque) { return ((struct console_fixture *)opaque)->input_ready; }
static int console_output_ready(void *opaque) { return ((struct console_fixture *)opaque)->output_ready; }
static int clock_get(void *opaque, uint32_t *v) { struct clock_fixture *f = opaque; if (f->fail) return -1; *v = f->value; return 0; }
static int clock_set(void *opaque, uint32_t v) { struct clock_fixture *f = opaque; if (f->set_fail) return -1; f->set_value = v; ++f->set_count; return 0; }
static void put16(uint8_t *m, uint32_t a, uint16_t v) { m[a]=(uint8_t)(v>>8); m[a+1u]=(uint8_t)v; }
static void put32(uint8_t *m, uint32_t a, uint32_t v) { m[a]=(uint8_t)(v>>24); m[a+1u]=(uint8_t)(v>>16); m[a+2u]=(uint8_t)(v>>8); m[a+3u]=(uint8_t)v; }

static void test_guest_memory(void)
{
    struct amtari_context ctx = {0}; uint8_t m[8]={0x12,0x34,0x56,0x78,0,0,0,0}; uint16_t v16=0; uint32_t v32=0;
    assert(amtari_init(&ctx)==0); assert(amtari_guest_memory_bind(&ctx,m,sizeof(m))==0);
    assert(amtari_guest_range_valid(&ctx,0,8)==1); assert(amtari_guest_range_valid(&ctx,7,2)==0);
    assert(amtari_guest_read16(&ctx,0,&v16)==0 && v16==0x1234u);
    assert(amtari_guest_read32(&ctx,0,&v32)==0 && v32==0x12345678u);
    assert(amtari_guest_read32(&ctx,6,&v32)==AMTARI_EFAULT);
    assert(amtari_guest_write16(&ctx,4,0xabcd)==0); assert(m[4]==0xabu && m[5]==0xcdu);
}

static void test_traps(void)
{
    struct amtari_context ctx={0};
    assert(amtari_trap_decode(1)==AMTARI_TRAP_GEMDOS); assert(amtari_trap_decode(2)==AMTARI_TRAP_GEM);
    assert(amtari_trap_decode(13)==AMTARI_TRAP_BIOS); assert(amtari_trap_decode(14)==AMTARI_TRAP_XBIOS);
    assert(amtari_trap_dispatch(&ctx,1,0xffffu)==AMTARI_EINVAL); assert(amtari_init(&ctx)==0);
    assert(amtari_trap_dispatch(&ctx,1,0xffffu)==AMTARI_ENOSYS); assert(amtari_trap_dispatch(&ctx,13,0)==AMTARI_ENOSYS);
    assert(amtari_trap_dispatch(&ctx,14,0)==AMTARI_ENOSYS); assert(amtari_trap_dispatch(&ctx,2,0)==AMTARI_ENOSYS);
}

static void test_bios_console_and_drvmap(void)
{
    struct amtari_context ctx={0}; struct console_fixture f={0x41,1,1,0,0}; uint8_t m[64]={0};
    assert(amtari_init(&ctx)==0); assert(amtari_guest_memory_bind(&ctx,m,sizeof(m))==0);
    assert(amtari_console_bind(&ctx,console_getc,console_putc,&f)==0); assert(amtari_console_status_bind(&ctx,console_input_ready,console_output_ready)==0); ctx.cpu.a[7]=0x10u;
    put16(m,0x10u,1); put16(m,0x12u,2); assert(amtari_trap_dispatch(&ctx,13u,1)==-1); f.input_ready=0; assert(amtari_trap_dispatch(&ctx,13u,1)==0); f.input_ready=1;
    put16(m,0x10u,2); put16(m,0x12u,2); assert(amtari_trap_dispatch(&ctx,13u,2)==0x41);
    put16(m,0x10u,3); put16(m,0x12u,2); put16(m,0x14u,0x5a); assert(amtari_trap_dispatch(&ctx,13u,3)==0); assert(f.output_count==1 && f.output=='Z');
    put16(m,0x10u,8); put16(m,0x12u,2); assert(amtari_trap_dispatch(&ctx,13u,8)==-1); f.output_ready=0; assert(amtari_trap_dispatch(&ctx,13u,8)==0); f.output_ready=1;
    put16(m,0x12u,1); assert(amtari_trap_dispatch(&ctx,13u,1)==AMTARI_ENOSYS); assert(amtari_trap_dispatch(&ctx,13u,2)==AMTARI_ENOSYS); assert(amtari_trap_dispatch(&ctx,13u,3)==AMTARI_ENOSYS); assert(amtari_trap_dispatch(&ctx,13u,8)==AMTARI_ENOSYS);
    assert(amtari_fs_set_drives(&ctx,5u,0u)==0); assert(amtari_trap_dispatch(&ctx,13u,0x0au)==5);
}

static void test_xbios_random(void)
{
    struct amtari_context ctx={0}; int32_t a,b,c; assert(amtari_init(&ctx)==0); assert(amtari_random_seed(&ctx,1)==0);
    a=amtari_trap_dispatch(&ctx,14u,0x11u); b=amtari_trap_dispatch(&ctx,14u,0x11u); c=amtari_trap_dispatch(&ctx,14u,0x11u);
    assert(a==0x00bb40e6); assert(b==0x005eb5ca); assert(c==0x004c4530); assert((a & ~0x00ffffff)==0); assert((b & ~0x00ffffff)==0); assert((c & ~0x00ffffff)==0);
    assert(amtari_random_seed(&ctx,1)==0); assert(amtari_trap_dispatch(&ctx,14u,0x11u)==a);
}

static void test_xbios_time(void)
{
    struct amtari_context ctx={0}; struct clock_fixture f={UINT32_C(0x5c4f7b1d),0u,0,0,0}; uint8_t m[64]={0};
    assert(amtari_init(&ctx)==0); assert(amtari_guest_memory_bind(&ctx,m,sizeof(m))==0); assert(amtari_trap_dispatch(&ctx,14u,0x17u)==AMTARI_EIO);
    assert(amtari_clock_bind(&ctx,clock_get,&f)==0); assert((uint32_t)amtari_trap_dispatch(&ctx,14u,0x17u)==f.value); f.value=UINT32_C(0xfc4f7b1d); assert((uint32_t)amtari_trap_dispatch(&ctx,14u,0x17u)==f.value);
    ctx.cpu.a[7]=0x10u; put16(m,0x10u,0x16u); put32(m,0x12u,UINT32_C(0x9abcdef0)); assert(amtari_trap_dispatch(&ctx,14u,0x16u)==AMTARI_EIO);
    assert(amtari_clock_bind_rw(&ctx,clock_get,clock_set,&f)==0); assert(amtari_trap_dispatch(&ctx,14u,0x16u)==0); assert(f.set_count==1 && f.set_value==UINT32_C(0x9abcdef0));
    f.set_fail=1; assert(amtari_trap_dispatch(&ctx,14u,0x16u)==AMTARI_EIO); f.set_fail=0; ctx.cpu.a[7]=62u; assert(amtari_trap_dispatch(&ctx,14u,0x16u)==AMTARI_EFAULT); f.fail=1; assert(amtari_trap_dispatch(&ctx,14u,0x17u)==AMTARI_EIO);
}

int main(void) { test_guest_memory(); test_traps(); test_bios_console_and_drvmap(); test_xbios_random(); test_xbios_time(); return 0; }
