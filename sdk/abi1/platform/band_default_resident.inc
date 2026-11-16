/* Resident B27: registered runtime and entry intentionally live until reboot. */
static __attribute__((always_inline)) inline void err(u32 p,u32 e){if(!RD(p+76u))RD(p+76u)=e;}
static __attribute__((noinline,section(".text"))) int queues_clear(u32 page,u32 lo,u32 hi){
 u32 n=RB(0x200d2061u);if(n>10u)return 0;for(u32 i=0;i<n;i++)if(RD(0x200d2064u+i*4u)==page)return 0;
 n=RB(0x200d208cu);if(n>10u)return 0;for(u32 i=0;i<n;i++)if(RD(0x200d2090u+i*4u)==page)return 0;
 if(RD(0x200c2d28u)!=8u)return 0;u32 a=RD(0x200c2d2cu),last=0;
 for(n=0;a&&n<256u;n++){if(!span(a,16u,lo,hi)||RD(a+8u)!=last||RD(a)==page)return 0;last=a;a=RD(a+12u);}
 return !a&&last==RD(0x200c2d30u);
}
/* Snapshot both group headers with bounded live links. Returns current top ID. */
static __attribute__((noinline,section(".text"))) u32 resident_top(u32 p,u32 lo,u32 hi){
 u32 top=0;
 for(u32 k=0;k<2u;k++){
  u32 h=k?0x200c2d34u:0x200c2d44u,g=RD(h+8u),prev=0,ng=0,np=0;
  if(RD(h+4u)!=48u){err(p,98);return 0;}
  for(;g&&ng<64u;ng++){
   if(!span(g,56u,lo,hi)||RD(g+48u)!=prev||RD(g+8u)!=h||RD(g+12u)!=4u){err(p,98);return 0;}
   u32 a=RD(g+16u),last=0,n=0;
   for(;a&&n<256u;n++){
    if(!span(a,12u,lo,hi)||RD(a+4u)!=last||!ram(RD(a),108u,lo,hi)){err(p,98);return 0;}
    u32 page=RD(a),id=RD(page+20u);if(id==0x7b1c0001u&&page!=p+256u){err(p,98);return 0;}
    if(!k)top=id;last=a;a=RD(a+8u);
   }
   if(a||RD(g+20u)!=last||RH(g+4u)!=n){err(p,98);return 0;}
   np+=n;prev=g;g=RD(g+52u);
  }
  if(g||RD(h+12u)!=prev||RH(h)!=ng||RH(h+2u)!=np){err(p,98);return 0;}
 }
 return top;
}
static inline u32 unaligned(u32 a);
static __attribute__((noinline,section(".text"))) u32 resident_lists(u32 p,u32 lo,u32 hi){
 u32 mask=0;
 for(u32 k=0;k<2u;k++){
  u32 h=0x20087334u+k*12u,st=k?129u:28u,a=RD(h+4u),last=0,n=0,found=0;
  if(RD(h)!=st){err(p,97);return 0;}
  for(;a&&n<256u;n++){
   if(!span(a,st+8u,lo,hi)||unaligned(a+st)!=last){err(p,97);return 0;}
   u32 same=1;
   if(!k)same=RH(a)==0x7b1cu;
   else for(u32 j=0;j<17u;j++)if(RB(a+j)!=RB(p+224u+j))same=0;
   if(same){
    if(found++){err(p,97);return 0;}
    if(!k&&(RD(a+8u)!=RD(p+140u)||RD(a+12u)!=p+948u)){err(p,97);return 0;}
    RD(p+720u+k*4u)=a;mask|=1u<<k;
   }
   last=a;a=unaligned(a+st+4u);
  }
  if(a||RD(h+8u)!=last){err(p,97);return 0;}
  if(!found)RD(p+720u+k*4u)=0;
 }
 return mask;
}
static __attribute__((noinline,section(".text"))) void resident_text(u32 p){ns6page_text((void*)(p+1024u),(void*)(p+2048u));}
static int reg_view(u32 p,u32 lo,u32 hi,u32*out);
static __attribute__((noinline,section(".text"))) int resident_app_chain(u32 p,u32 lo,u32 hi){
 u32 head=0x20084b84u,a=RD(head+4u),last=head,n=0,found=0;
 if(RD(0x20084e58u)!=head)return 0;
 for(;a!=head&&n<256u;n++){
  if(!ram(a,80u,lo,hi)||RD(a)!=last)return 0;
  if(RH(a+20u)==0x7b1cu){if(a!=RD(p+136u)||found++)return 0;}
  last=a;a=RD(a+4u);
 }
 return a==head&&RD(head)==last&&n==RH(0x20084e60u)&&found==1u;
}
static __attribute__((noinline,section(".text"))) void resident_poll(u32 p,u32 lo,u32 hi){
 u32 rv[4];if(!resident_app_chain(p,lo,hi)||reg_view(p,lo,hi,rv)||rv[0]!=1u||rv[1]){err(p,96);return;}
 u32 app=((u32(*)(u32))0xc6aa995u)(0x7b1cu);
 RD(p+812u)=0;
 if(!app||app!=RD(p+136u)){err(p,96);return;}
 RD(p+812u)=1;
 if(((u32(*)(u32))0xc693e8du)(0x7b1c0001u)!=p+256u){err(p,96);return;}
 RD(p+812u)|=2u;
 RD(p+816u)=resident_lists(p,lo,hi);if(RD(p+76u))return;
 u32 top=resident_top(p,lo,hi);RD(p+596u)=0;if(RD(p+76u))return;
 u32 n=RD(p+560u),complete=n&&RD(p+564u)==n&&RD(p+568u)==n&&RD(p+572u)==n&&RD(p+576u)==n&&RD(p+580u)==n&&RD(p+608u)==n&&RD(p+612u)==n;
 if(complete&&!RD(p+304u)&&!RD(p+292u)&&queues_clear(p+256u,lo,hi))RD(p+808u)=n;
 if(top==0x100000u&&RD(0x200c2d44u)==0x10001u&&!RD(0x200c2d34u)&&RB(0x20087248u)==4u&&!RD(0x20087250u)&&!RD(p+304u)&&!RD(p+292u))RD(p+596u)=1;


}
__attribute__((used,section(".text"),aligned(4))) void page_create(u32 page,u32 root,u32 arg){
 (void)arg;
 u32 p=page-256u;if(RD(p)!=0xb641c0deu)return;
 if(!ui(p)||!root||RD(page+48u)!=root){err(p,72);return;}
 resident_text(p);
 RD(p+552u)=root;((void(*)(u32,u32))0xc38221bu)(root,1u);
 u32 label=((u32(*)(u32))0xc3ab22du)(root);RD(p+556u)=label;
 if(!label)err(p,73);
 else{((void(*)(u32,u32))0xc3b38b1u)(label,p+2048u);RD(p+640u)=RD(label+52u);if(!RD(p+640u)||RD(p+640u)==p+2048u)err(p,74);((void(*)(u32,u32,u32,int,int))0xc383f3du)(label,root,9u,0,0);}
 RD(p+2520u)=0;RD(p+560u)++;
}
__attribute__((used,section(".text"),aligned(4))) void page_edge(u32 page){u32 p=page-256u;if(RD(p)!=0xb641c0deu)return;if(!ui(p)){err(p,72);return;}u32 s=RB(page+40u);if(s==5u)RD(p+568u)++;else if(s==8u||s==18u)RD(p+572u)++;else err(p,75);}
__attribute__((used,section(".text"),aligned(4))) void page_destroy(u32 page){u32 p=page-256u;if(RD(p)!=0xb641c0deu)return;if(!ui(p)){err(p,72);return;}ns5page_pause((void*)(p+1024u));RD(p+564u)++;if(RD(page+48u)&&RD(page+48u)==RD(p+552u))RD(p+600u)=1;else err(p,76);RD(p+556u)=0;}
__attribute__((used,section(".text"),aligned(4))) void page_resume(u32 page){u32 p=page-256u;if(RD(p)!=0xb641c0deu)return;if(!ui(p)||(RB(page+40u)!=8u&&RB(page+40u)!=18u)){err(p,77);return;}RD(p+576u)++;ns5page_resume((void*)(p+1024u));}
__attribute__((used,section(".text"),aligned(4))) void page_pause(u32 page){u32 p=page-256u;if(RD(p)!=0xb641c0deu)return;if(!ui(p)||RB(page+40u)!=17u){err(p,78);return;}RD(p+580u)++;ns5page_pause((void*)(p+1024u));}
