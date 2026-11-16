#include "g2_fixture.h"
#include "ns_port.h"
#include "runtime_image.h"
#define W(a) (*(volatile ns_u32*)(a))
#define F(a,t) ((t)((a)|1u))
extern void copy_runtime_image(void*);
int probe_main(volatile ns_u32*m){
 if(W(0x200b2330)!=0x3c356b40)return -1;
 ns_u32 lo=W(0x3c356b5c),hi=W(0x3c356b60),v=(ns_u32)m;
 if((v&3)||v<=lo||v>hi||hi-v<128)return -2;
 m[0]=G2_TAG;
 if(!m[1]){
  if(m[2]){m[30]=1;return -3;}
  struct g2_fixture*g=F(0xc3514ec,void*(*)(ns_u32))(32768);m[2]=(ns_u32)g;
  ns_u32 allocated=(ns_u32)g;
  if(!g||(allocated&3)||allocated<=lo||allocated>hi||hi-allocated<32768){m[30]=2;return -4;}
  for(ns_u32 i=0;i<2048/4;i++)W((ns_u32)g+i*4)=0;
  g->tag=G2_TAG;copy_runtime_image((void*)((ns_u32)g+2048));
  F(0xc92e6da,void(*)(void))();F(0xc91ca02,void(*)(void))();F(0xc0c181e,void(*)(void))();F(0xc91ca02,void(*)(void))();
  ns_u32 cb=((ns_u32)g+2048-0x20000000+G1_TICK)|1;
  g->timer=F(0xc3abc88,void*(*)(ns_u32,ns_u32,void*))(cb,100,g);
  if(!g->timer){m[30]=3;return -5;}m[1]=1;
 }
 ns_u32 p=m[2];if((p&3)||p<=lo||p>hi||hi-p<32768||W(p)!=G2_TAG){m[30]=4;return -6;}
 struct g2_fixture*g=(void*)p;__atomic_fetch_add(&g->heartbeat,1,__ATOMIC_ACQ_REL);
 if(m[1]==2)g->cancel=1;
 if(m[1]==3){
  for(ns_u32 i=3;i<32;i++)m[i]=0;
  for(ns_u32 i=0;i<22;i++)m[3+i]=g->diagnostics[i];return 0;
 }
 for(ns_u32 i=0;i<26;i++)m[3+i]=g->report[i];m[29]=g->timer!=0;
 /* No runtime free: callback registrations have not been retired. */
 return 0;
}
