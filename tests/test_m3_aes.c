#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "amtari.h"

struct timer_fixture { int calls; uint32_t ms; };
struct key_fixture { int ready, key, reads; };
struct mouse_fixture { int ready; int16_t x, y; uint16_t buttons, kstate; };

static int timer(void *o, uint32_t ms) { struct timer_fixture *f=o; f->calls++; f->ms=ms; return 0; }
static int kready(void *o) { return ((struct key_fixture*)o)->ready; }
static int kget(void *o) { struct key_fixture *f=o; f->reads++; f->ready=0; return f->key; }
static int kput(void *o, unsigned char c) { (void)o; (void)c; return 0; }
static int mouse(void *o,int16_t*x,int16_t*y,uint16_t*b,uint16_t*k) { struct mouse_fixture*f=o; if(!f->ready)return 0; *x=f->x;*y=f->y;*b=f->buttons;*k=f->kstate;return 1; }
static void w16(uint8_t*m,uint32_t a,uint16_t v){m[a]=(uint8_t)(v>>8);m[a+1]=(uint8_t)v;}
static void w32(uint8_t*m,uint32_t a,uint32_t v){w16(m,a,(uint16_t)(v>>16));w16(m,a+2,(uint16_t)v);}
static void apb(uint8_t*m,uint16_t op,uint16_t ni,uint16_t no,uint16_t nai){w32(m,0x300,0x340);w32(m,0x304,0x360);w32(m,0x308,0x380);w32(m,0x30c,0x3c0);w32(m,0x310,0x400);w32(m,0x314,0x440);w16(m,0x340,op);w16(m,0x342,ni);w16(m,0x344,no);w16(m,0x346,nai);w16(m,0x348,0);}
static void trap2(struct amtari_context*c,uint8_t*m){w16(m,0x20,0x4e42);c->cpu.pc=0x20;c->cpu.d[0]=0xc8;c->cpu.d[1]=0x300;assert(amtari_exec_step(c)==AMTARI_EXEC_RUNNING);}
static uint16_t r16(struct amtari_context*c,uint32_t a){uint16_t v=0;assert(!amtari_guest_read16(c,a,&v));return v;}

int main(void){
 struct amtari_context c={0}; uint8_t mem[2048]; uint16_t id; unsigned i;
 struct timer_fixture t={0}; struct key_fixture key={1,0x1e61,0}; struct mouse_fixture m={1,123,77,1,4};
 assert(!amtari_init(&c)); memset(mem,0,sizeof(mem)); assert(!amtari_guest_memory_bind(&c,mem,sizeof(mem)));
 assert(!amtari_aes_set_application_name(&c,"AMTARI"));
 /* appl_init */ apb(mem,10,0,1,0);trap2(&c,mem);id=r16(&c,0x3c0);assert(id==1);
 /* appl_find */ memcpy(mem+0x500,"AMTARI",7);apb(mem,13,0,1,1);w32(mem,0x400,0x500);trap2(&c,mem);assert(r16(&c,0x3c0)==id);
 /* appl_write + appl_read */ for(i=0;i<16;i++)mem[0x520+i]=(uint8_t)(0xa0+i);apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);assert(r16(&c,0x3c0)==1);memset(mem+0x540,0,16);apb(mem,11,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x540);trap2(&c,mem);assert(r16(&c,0x3c0)==1);assert(!memcmp(mem+0x520,mem+0x540,16));
 /* evnt_mesag */ apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);memset(mem+0x560,0,16);apb(mem,23,0,1,1);w32(mem,0x400,0x560);trap2(&c,mem);assert(r16(&c,0x3c0)==1);assert(!memcmp(mem+0x520,mem+0x560,16));
 /* evnt_timer */ assert(!amtari_aes_timer_bind(&c,timer,&t));apb(mem,24,2,1,0);w16(mem,0x380,25);w16(mem,0x382,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&t.ms==25);
 /* MU_MESAG */ apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);apb(mem,25,16,7,1);w16(mem,0x380,0x0010);w32(mem,0x400,0x580);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0010);assert(!memcmp(mem+0x520,mem+0x580,16));
 /* MU_KEYBD */ assert(!amtari_console_bind(&c,kget,kput,&key));assert(!amtari_console_status_bind(&c,kready,0));apb(mem,25,16,7,0);w16(mem,0x380,0x0001);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0001&&r16(&c,0x3ca)==0x1e61);
 /* MU_BUTTON */ assert(!amtari_aes_mouse_bind(&c,mouse,&m));apb(mem,25,16,7,0);w16(mem,0x380,0x0002);w16(mem,0x382,1);w16(mem,0x384,1);w16(mem,0x386,1);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0002&&r16(&c,0x3c2)==123&&r16(&c,0x3c4)==77);
 /* MU_M1 */ apb(mem,25,16,7,0);w16(mem,0x380,0x0004);w16(mem,0x388,0);w16(mem,0x38a,100);w16(mem,0x38c,50);w16(mem,0x38e,50);w16(mem,0x390,50);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0004);
 /* MU_M2 */ apb(mem,25,16,7,0);w16(mem,0x380,0x0008);w16(mem,0x392,0);w16(mem,0x394,100);w16(mem,0x396,50);w16(mem,0x398,50);w16(mem,0x39a,50);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0008);
 /* MU_TIMER fallback */ m.x=10;m.y=10;apb(mem,25,16,7,0);w16(mem,0x380,0x0024);w16(mem,0x388,0);w16(mem,0x38a,100);w16(mem,0x38c,50);w16(mem,0x38e,50);w16(mem,0x390,50);w16(mem,0x39c,7);w16(mem,0x39e,0);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0020&&t.ms==7);
 /* appl_exit */ apb(mem,19,0,1,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!c.aes.application_active);
 return 0;
}
