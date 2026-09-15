#include "amtari.h"

int amtari_vdi_bind(struct amtari_context *ctx, amtari_vdi_dispatch_fn dispatch_fn, void *opaque)
{ if(ctx==0||!ctx->initialized)return AMTARI_EINVAL; ctx->vdi.dispatch=dispatch_fn; ctx->vdi.opaque=opaque; return 0; }
int amtari_vdi_line_bind(struct amtari_context *ctx, amtari_vdi_line_fn line_fn, void *opaque)
{ if(ctx==0||!ctx->initialized)return AMTARI_EINVAL; ctx->vdi.line=line_fn; ctx->vdi.line_opaque=opaque; return 0; }
int amtari_vdi_configure(struct amtari_context *ctx,uint16_t width,uint16_t height,uint16_t colors)
{ if(ctx==0||!ctx->initialized||width==0u||height==0u||colors==0u)return AMTARI_EINVAL; ctx->vdi.width=width;ctx->vdi.height=height;ctx->vdi.colors=colors;if(ctx->vdi.next_handle==0u)ctx->vdi.next_handle=1u;if(ctx->vdi.line_style==0u)ctx->vdi.line_style=1u;return 0; }
/* Classic VDI v_opnwk returns work_out[0..44] through intout and
 * work_out[45..56] through ptsout. Keep unsupported capability fields zero. */
static int32_t vdi_open_workstation(struct amtari_context *ctx,int16_t *intout,uint16_t ic,uint16_t *ni,int16_t *ptsout,uint16_t pc,uint16_t *np)
{ uint16_t i;if(ctx->vdi.width==0u||ctx->vdi.height==0u||ctx->vdi.colors==0u)return AMTARI_ENOSYS;if(ic<45u||pc<12u||!intout||!ptsout)return AMTARI_EINVAL;for(i=0u;i<45u;++i)intout[i]=0;for(i=0u;i<12u;++i)ptsout[i]=0;intout[0]=(int16_t)(ctx->vdi.width-1u);intout[1]=(int16_t)(ctx->vdi.height-1u);intout[3]=1;intout[4]=1;intout[5]=1;intout[6]=1;intout[7]=1;intout[8]=1;intout[9]=1;intout[10]=1;intout[13]=(int16_t)ctx->vdi.colors;intout[35]=1;*ni=45u;*np=12u;ctx->vdi.next_handle++;return 0; }
static int32_t vdi_line_attribute(struct amtari_context *ctx,uint16_t opcode,const int16_t *intin,uint16_t n,int16_t *intout,uint16_t cap,uint16_t *count)
{ uint16_t v;if(!intin||n<1u||!intout||cap<1u)return AMTARI_EINVAL;v=(uint16_t)intin[0];if(opcode==15u){if(v<1u||v>6u)v=1u;ctx->vdi.line_style=v;}else{if(v>=ctx->vdi.colors)v=1u;ctx->vdi.line_color=v;}intout[0]=(int16_t)v;*count=1u;return 0; }
static int32_t vdi_polyline(struct amtari_context *ctx,const int16_t *ptsin,uint16_t n)
{ uint16_t i;if(!ctx->vdi.line)return AMTARI_ENOSYS;if(!ptsin||n<4u||(n&1u)!=0u)return AMTARI_EINVAL;for(i=0u;i+3u<n;i=(uint16_t)(i+2u)){if(ctx->vdi.line(ctx->vdi.line_opaque,ptsin[i],ptsin[i+1u],ptsin[i+2u],ptsin[i+3u],ctx->vdi.line_color,ctx->vdi.line_style)!=0)return AMTARI_EIO;}return 0; }
int32_t amtari_vdi_dispatch(struct amtari_context *ctx,uint16_t opcode,const int16_t *intin,uint16_t intin_count,const int16_t *ptsin,uint16_t ptsin_count,int16_t *intout,uint16_t intout_capacity,uint16_t *intout_count,int16_t *ptsout,uint16_t ptsout_capacity,uint16_t *ptsout_count)
{ int32_t rc;if(!ctx||!ctx->initialized||!intout_count||!ptsout_count)return AMTARI_EINVAL;*intout_count=0u;*ptsout_count=0u;if(opcode==1u)return vdi_open_workstation(ctx,intout,intout_capacity,intout_count,ptsout,ptsout_capacity,ptsout_count);if(opcode==6u)return vdi_polyline(ctx,ptsin,ptsin_count);if(opcode==15u||opcode==17u)return vdi_line_attribute(ctx,opcode,intin,intin_count,intout,intout_capacity,intout_count);if(!ctx->vdi.dispatch)return AMTARI_ENOSYS;rc=ctx->vdi.dispatch(ctx->vdi.opaque,opcode,intin,intin_count,ptsin,ptsin_count,intout,intout_capacity,intout_count,ptsout,ptsout_capacity,ptsout_count);if(rc!=0)return rc;if(*intout_count>intout_capacity||*ptsout_count>ptsout_capacity)return AMTARI_EINVAL;return 0; }

/* M3.9 AES host abstraction. Guest AESPB/TRAP #2 routing follows in M3.10. */
int amtari_aes_bind(struct amtari_context *ctx,amtari_aes_dispatch_fn dispatch_fn,void *opaque)
{ if(ctx==0||!ctx->initialized)return AMTARI_EINVAL;ctx->aes.dispatch=dispatch_fn;ctx->aes.opaque=opaque;return 0; }
int32_t amtari_aes_dispatch(struct amtari_context *ctx,uint16_t opcode,const int16_t *intin,uint16_t intin_count,int16_t *intout,uint16_t intout_capacity,uint16_t *intout_count,const uint32_t *addrin,uint16_t addrin_count,uint32_t *addrout,uint16_t addrout_capacity,uint16_t *addrout_count)
{ int32_t rc;if(ctx==0||!ctx->initialized||intout_count==0||addrout_count==0)return AMTARI_EINVAL;*intout_count=0u;*addrout_count=0u;if(ctx->aes.dispatch==0)return AMTARI_ENOSYS;rc=ctx->aes.dispatch(ctx->aes.opaque,opcode,intin,intin_count,intout,intout_capacity,intout_count,addrin,addrin_count,addrout,addrout_capacity,addrout_count);if(rc!=0)return rc;if(*intout_count>intout_capacity||*addrout_count>addrout_capacity)return AMTARI_EINVAL;return 0; }
