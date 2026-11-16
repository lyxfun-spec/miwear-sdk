/* B27 checked transient launcher records, no UI-list observer dispatch.
 * Primary data28+links8; secondary embedded name128/flags1+links8.
 * Exactly the native in-RAM record shapes; secondary stride is129, not132. */
static __attribute__((always_inline)) inline u32 unaligned(u32 a){return (u32)RB(a)|((u32)RB(a+1u)<<8)|((u32)RB(a+2u)<<16)|((u32)RB(a+3u)<<24);}
static __attribute__((always_inline)) inline void put_unaligned(u32 a,u32 v){for(u32 j=0;j<4u;j++)RB(a+j)=(u8)(v>>(j*8u));}
/* Excludes exactly the own node to compare the original list projection.
 * out first,last,count,digest,owncount,first immutable ROM icon. */
static __attribute__((noinline,section(".text"))) u32 launch_view(u32 p,u32 which,u32 own,u32 lo,u32 hi,u32*out){
 u32 head=0x20087334u+which*12u,stride=which?129u:28u,a=RD(head+4u),last=0,n=0,h=2166136261u;
 for(u32 i=0;i<6u;i++)out[i]=0;
 if(RD(head)!=stride)return 81;
 while(a){
  if(n++>=256u||!span(a,stride+8u,lo,hi)||unaligned(a+stride)!=last)return 82;
  u32 prev=unaligned(a+stride),next=unaligned(a+stride+4u),actualnext=next;
  if(a==own){out[4]++;if(out[4]>1u)return 82;}
  else{
   if(!which&&RH(a)==0x7b1cu)return 83;
   if(which){u32 same=1;for(u32 j=0;j<17u;j++)if(RB(a+j)!=RB(p+224u+j))same=0;if(same)return 83;}
   if(own&&prev==own)prev=unaligned(own+stride);if(own&&next==own)next=unaligned(own+stride+4u);
   if(!out[2])out[0]=a;out[1]=a;out[2]++;
   h=mix(mix(mix(h,a),prev),next);
   for(u32 j=0;j<stride;j++)h=mix(h,RB(a+j));

  }
  last=a;a=actualnext;
 }
 if(last!=RD(head+8u))return 82;
 out[3]=h;return 0;
}
static __attribute__((noinline,section(".text"))) u32 launch_guard(u32 p){
 if(RD(0x200c5d78u)!=0xc5415ddu||RD(0x200c5d7cu)!=0xc5415cdu||RD(0x200c5d80u)!=0xc548329u||RD(0x200c5d84u)!=0xc5489a9u)return 84;
 for(u32 i=0;i<8u;i++)if(RD(0x20087298u+i*4u)!=RD(0x2cc5276cu+i*4u))return 84;
 for(u32 i=0;i<2u;i++)if(RD(0x200872d0u+i*4u)!=RD(0x2cc527a4u+i*4u))return 84;
 if(RB(0x20087248u)!=4u||RD(0x20087250u))return 85;
 u32 style=RB(0x200c5b3fu)|((u32)RB(0x200c5b3eu)<<8);
 if(RD(p+796u)&&RD(p+800u)!=style)return 84;
 RD(p+800u)=style;RD(p+796u)=1;return 0;
}
static __attribute__((noinline,section(".text"))) void launch_private_free(u32 p){
 if(RD(p+728u))return;
 if(RD(p+724u))((void(*)(u32))0xc34cc7du)(RD(p+724u));RD(p+724u)=0;
 if(RD(p+720u))((void(*)(u32))0xc34cc7du)(RD(p+720u));RD(p+720u)=0;
}
static __attribute__((noinline,section(".text"))) u32 launch_prepare(u32 p,u32 lo,u32 hi){
 u32 e=launch_guard(p),v[6];if(e)return e;
 for(u32 k=0;k<2u;k++){
  e=launch_view(p,k,0,lo,hi,v);if(e)return e;
  u32 base=k?756u:740u;for(u32 j=0;j<4u;j++)RD(p+base+j*4u)=v[j];
  if(!k)RD(p+736u)=v[5];
 }
 /* Fixed firmware settings resource filename; cached icon fields remain opaque. */
 RD(p+948u)=0x7365722fu;
 RD(p+952u)=0x6372756fu;
 RD(p+956u)=0x70612f65u;
 RD(p+960u)=0x6f632f70u;
 RD(p+964u)=0x6f72746eu;
 RD(p+968u)=0x65635f6cu;
 RD(p+972u)=0x7265746eu;
 RD(p+976u)=0x6f63692fu;
 RD(p+980u)=0x65732f6eu;
 RD(p+984u)=0x6e697474u;
 RD(p+988u)=0x622e7367u;
 RD(p+992u)=0x00006e69u;
 for(u32 j=0;j<47u;j++)if(RB(0x2cb63dfdu+j)!=RB(p+948u+j))return 86;
 RD(p+736u)=p+948u;RD(p+804u)=3;
 u32 cb=RD(p+732u),start=p+4096u-0x20000000u;
 if(!(cb&1u)||(cb&~1u)<start||(cb&~1u)>=start+28672u)return 87;
 u32 a=((u32(*)(u32))0xc3514edu)(36u);RD(p+720u)=a;if(!a)return 88;
 if(!span(a,36u,lo,hi)||!(a+36u<=p||a>=p+32768u)){RD(p+728u)|=4u;return 89;}
 u32 b=((u32(*)(u32))0xc3514edu)(137u);RD(p+724u)=b;if(!b)return 90;
 if(!span(b,137u,lo,hi)||!(b+137u<=p||b>=p+32768u)||!(a+36u<=b||a>=b+137u)){RD(p+728u)|=4u;return 89;}
 for(u32 j=0;j<36u;j++)RB(a+j)=0;for(u32 j=0;j<137u;j++)RB(b+j)=0;
 RD(p+928u)=0x20313142u; RD(p+932u)=0x0036534eu;
 RD(RD(p+136u)+16u)=p+948u;RD(RD(p+136u)+36u)=cb;
 RH(a)=0x7b1cu;RD(a+4u)=p+928u;RD(a+8u)=RD(p+140u);RD(a+12u)=p+948u;RB(a+24u)=4u;
 for(u32 j=0;j<17u;j++)RB(b+j)=RB(p+224u+j);RB(b+128u)=10u;
 return 0;
}
static __attribute__((noinline,section(".text"))) u32 launch_live(u32 p,u32 lo,u32 hi){
 u32 e=launch_guard(p),v[6],mask=RD(p+728u);if(e)return e;
 for(u32 k=0;k<2u;k++){
  u32 own=RD(p+720u+k*4u),base=k?756u:740u;
  e=launch_view(p,k,own,lo,hi,v);if(e)return e;
  if(v[4]!=((mask>>k)&1u))return 91;
  for(u32 j=0;j<4u;j++)if(v[j]!=RD(p+base+j*4u))return 92;
  if(own){
   if(!k){if(RH(own)!=0x7b1cu||RD(own+4u)!=p+928u||RD(own+8u)!=RD(p+140u)||RD(own+12u)!=p+948u||RB(own+24u)!=4u)return 93;}
   else{for(u32 j=0;j<128u;j++)if(RB(own+j)!=(j<17u?RB(p+224u+j):0u))return 93;if(RB(own+128u)!=10u)return 93;}
  }
 }
 return 0;
}
static __attribute__((noinline,section(".text"))) u32 launch_publish(u32 p,u32 lo,u32 hi){
 u32 e=launch_live(p,lo,hi);if(e)return e;
 if(((u32(*)(u32))0xc69f91du)(0x7b1cu)!=p+928u)return 94;
 RD(p+788u)=1;
 for(u32 k=0;k<2u;k++){
  u32 h=0x20087334u+k*12u,stride=k?129u:28u,a=RD(p+720u+k*4u),tail=RD(h+8u);
  put_unaligned(a+stride,tail);put_unaligned(a+stride+4u,0);
  __asm__ volatile("dsb sy" ::: "memory");
  if(tail)put_unaligned(tail+stride+4u,a);else RD(h+4u)=a;
  RD(h+8u)=a;RD(p+728u)|=1u<<k;
 }
 e=launch_live(p,lo,hi);if(e)return e;RD(p+772u)=1;return 0;
}
__attribute__((used,section(".text.icon"),aligned(4))) u32 title_provider(u32 original){
 u32 p;__asm__ volatile("adr.w %0, owned_callback":"=r"(p));p=(p&~1u)+0x20000000u-4096u;
 if(RD(p)!=0xb641c0deu)return 0;
 if(original!=RD(p+136u)||!ui(p)){err(p,95);return 0;}
 RD(p+784u)++;return p+928u;
}

