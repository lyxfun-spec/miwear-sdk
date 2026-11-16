#include "ns_sdk_page.h"
ns_u32 ns6page_pass(const struct ns_sdk_page *p,ns_u32 c,ns_u32 d,ns_u32 r,ns_u32 a,ns_u32 quiet,ns_u32 entry) {
 const struct ns_ble_backend *b=&p->ble;ns_u32 n=b->created;
 return p->flow.initialized && !p->flow.visible && !p->flow.error && !b->error && n>=3u && n==b->joined && n==b->released &&
  b->scan_start==n && b->scan_stop==n && b->adv_start==n && b->adv_stop==n && b->barriers==n && b->exit_proofs==n &&
  b->callbacks>=4u*n && b->overlaps && b->quiet_checks>=n && !b->owner &&
  ns_session_quiescent(&b->controller.manager) && c>=3u && c==d && r>=3u && r==a && quiet && entry==7u;
}
/* Packed immediate characters keep the raw-PIC UI formatter independent of
 * string literal/GOT data aliases. No libc formatting or worker UI access. */
static __attribute__((noinline)) void pgword(char *out,ns_u32 *n,ns_u32 value) {
 for(ns_u32 i=0;i<4u;i++){unsigned char c=(unsigned char)(value>>(8u*i));if(!c)return;if(*n<126u)out[(*n)++]=(char)c;}
}
static __attribute__((noinline)) void pgnum(char *out,ns_u32 *n,ns_u32 v) {
 char digits[10];ns_u32 len=0;do{digits[len++]=(char)('0'+v%10u);v/=10u;}while(v && len<10u);
 while(len && *n<126u)out[(*n)++]=digits[--len];
}
void ns6page_text(const struct ns_sdk_page *p,char out[128]) {
 ns_u32 n=0;pgword(out,&n,0x2036534eu);pgword(out,&n,0x4954414eu);pgword(out,&n,0x000a4556u);
 struct nss_snapshot snap={0};if(ns5page_observe(p,&snap)!=NS_OK){out[0]=0;return;}ns_u32 state=snap.state;
 if(p->flow.error || p->ble.error){pgword(out,&n,0x43454843u);pgword(out,&n,0x00000a4bu);}
 else if(snap.view==NSS_WAIT_PRIOR){pgword(out,&n,0x54494157u);pgword(out,&n,0x49525020u);pgword(out,&n,0x000a524fu);}
 else if(state==NS_ACTIVE){pgword(out,&n,0x44414552u);pgword(out,&n,0x00000a59u);}
 else{pgword(out,&n,0x52415453u);pgword(out,&n,0x474e4954u);pgword(out,&n,0x0000000au);}
 pgword(out,&n,0x3a6e6547u);pgnum(out,&n,p->flow.wanted_epoch);pgword(out,&n,0x4341420au);pgword(out,&n,0x6f74204bu);pgword(out,&n,0x6f747320u);pgword(out,&n,0x00000a70u);
 pgword(out,&n,0x706f6552u);pgword(out,&n,0x61206e65u);pgword(out,&n,0x72657466u);pgword(out,&n,0x43414220u);pgword(out,&n,0x0000004bu);out[n]=0;
}
