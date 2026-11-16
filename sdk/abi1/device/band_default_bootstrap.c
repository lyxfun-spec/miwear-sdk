#include "ns_sdk_page.h"
#include "default_image.h"
#define RD(a) (*(volatile ns_u32*)(a))
#define FN(a,t) ((t)((a)|1u))
extern void copy_runtime_image(void*);
static int span(ns_u32 p,ns_u32 n,ns_u32 lo,ns_u32 hi){return !(p&3u)&&p>lo&&p<=hi&&n<=hi-p;}
static ns_u32 code_hash(ns_u32 p){ns_u32 h=2166136261u;for(ns_u32 i=0;i<NS6_IMAGE_SIZE;i++)h=(h^*(volatile unsigned char*)(p+4096u+i))*16777619u;return h;}
static int owned(ns_u32 p,ns_u32 timer,ns_u32 lo,ns_u32 hi){
 if(!span(p,32768u,lo,hi)||RD(p)!=0xb641c0deu)return 0;
 if(RD(p+12u)!=timer||RD(timer+8u)!=((p+4096u-0x20000000u)|1u)||code_hash(p)!=NS6_IMAGE_HASH)return -1;
 return 1;
}
int probe_main(volatile ns_u32 *m){
 if(RD(0x200b2330u)!=0x3c356b40u)return -1;
 ns_u32 lo=RD(0x3c356b5cu),hi=RD(0x3c356b60u);if(!span((ns_u32)m,128u,lo,hi))return -2;
 m[0]=0x4e533642u;m[28]=0;
 /* Attach only through bounded live timer records, never by sweeping heap or
  * trusting a saved pointer. This manager has no unload operation. */
 if(RD(0x200bd108u)!=24u){m[28]=1;return -1;}
 ns_u32 t=RD(0x200bd10cu),prev=0,p=0,n=0;
 while(t && n++<8192u){
  if(!span(t,32u,lo,hi)||RD(t+24u)!=prev){m[28]=2;return -1;}
  ns_u32 q=RD(t+12u);int k=owned(q,t,lo,hi);
  if(k<0 || (k && p)){m[28]=3;return -1;}if(k)p=q;
  prev=t;t=RD(t+28u);
 }
 if(t||prev!=RD(0x200bd110u)){m[28]=4;return -1;}
 m[26]=p?1u:0u;
 if(!p){
  if(m[1]){m[28]=5;return -1;}
  p=(ns_u32)FN(0xc3514ecu,void*(*)(ns_u32))(32768u);
  if(!span(p,32768u,lo,hi)){m[28]=6;return -1;}
  for(ns_u32 i=0;i<4096u;i+=4u)RD(p+i)=0;
  RD(p)=0xb641c0deu;RD(p+56u)=RD(0x2010d9fcu);RD(p+60u)=RD(0x2010da00u);
  copy_runtime_image((void*)(p+4096u));
  FN(0xc92e6dau,void(*)(void))();FN(0xc91ca02u,void(*)(void))();FN(0xc0c181eu,void(*)(void))();FN(0xc91ca02u,void(*)(void))();
  ns_u32 code=p+4096u-0x20000000u;
  RD(p+700u)=(code+NS6_CREATE)|1u;RD(p+704u)=(code+NS6_EDGE)|1u;
  RD(p+708u)=(code+NS6_DESTROY)|1u;RD(p+712u)=(code+NS6_RESUME)|1u;RD(p+716u)=(code+NS6_PAUSE)|1u;
  RD(p+732u)=(code+NS6_TITLE)|1u;RD(p+144u)=(code+NS6_SIGNAL)|1u;RD(p+408u)=(code+NS6_PAGESIGNAL)|1u;
  RD(p+12u)=(ns_u32)FN(0xc3abc88u,void*(*)(ns_u32,ns_u32,void*))(code|1u,100u,(void*)p);
  if(!RD(p+12u)){FN(0xc34cc7cu,void(*)(void* ))((void*)p);m[28]=7;return -1;}
 }
 m[2]=p;m[3]=RD(p+12u);
 for(ns_u32 i=0;i<20u;i++)m[4u+i]=RD(p+2304u+i*4u);
 m[24]=1u;m[25]=m[19];m[1]=1u;return 0;
}
