static u32 ui_queued(void){
 u32 keep[20],release[20],original[20],nk=0,nr=0;
 s.record_error=0;
 i32 n=*(volatile signed char*)0x3c3e895c;
 if(n<0||n>10){s.record_error=1;return 1;}
 for(i32 i=0;i<n;i++)if(word(0x3c3e892c+4u*(u32)i)==(u32)s.page)return 1;
 n=*(volatile signed char*)0x3c3e895d;
 if(n<0||n>10){s.record_error=1;return 1;}
 for(i32 i=0;i<n;i++)if(word(0x3c3e84fc+4u*(u32)i)==(u32)s.page)return 1;
 n=*(volatile signed char*)0x3c3e84f8;
 if(n<0||n>20){s.record_error=2;return 1;}
 /* Validate the whole table before touching a count, pointer or allocation. */
 for(i32 i=0;i<n;i++){
  u32 p=word(0x3c3e84a8+4u*(u32)i);original[i]=p;
  if(!span(p,8)){s.record_error=3;return 1;}
  for(i32 j=0;j<i;j++)if(original[j]==p){s.record_error=4;return 1;}
  if(word(p)==(u32)s.page){
   if(!s.last_root||!s.ui_destroy||word(p+4)!=s.last_root){s.record_error=5;return 1;}
   release[nr++]=p;
  }else keep[nk++]=p;
 }
 if(nr){
  /* UI-serialized compaction first; free our rows after publishing new count. */
  for(u32 i=0;i<nk;i++)*(volatile u32*)(0x3c3e84a8+4*i)=keep[i];
  for(u32 i=nk;i<(u32)n;i++)*(volatile u32*)(0x3c3e84a8+4*i)=0;
  *(volatile signed char*)0x3c3e84f8=(signed char)nk;
  for(u32 i=0;i<nr;i++)FREE((void*)release[i]);
  s.rows_removed+=nr;
 }
 return 0;
}
static u32 pending(void){
 u32 head=0x3c21c25c,p=word(head+4),prev=head,n=0;
 while(p!=head&&n<1024){
  if(!span(p,12)||word(p)!=prev)return 1;
  u32 event=word(p+8);if(!span(event,12))return 1;
  if(word(event+4)==(u32)s.event_name)return 1;
  prev=p;p=word(p+4);++n;
 }
 return p!=head||word(head)!=prev;
}

static u32 bytes_word(u32 p){volatile unsigned char *b=(volatile unsigned char*)p;return b[0]|((u32)b[1]<<8)|((u32)b[2]<<16)|((u32)b[3]<<24);}
static u32 own_name(u32 p,u32 bound){
 for(u32 i=0;i<sizeof(appname);i++){
  if(i>=bound||*(volatile unsigned char*)(p+i)!=(unsigned char)appname[i])return 0;
 }return 1;
}
/* Check both linked-list integrity and unique ID/name ownership. */
static u32 launcher_list(u32 head,u32 order,u32 *matches){
 u32 n=word(head),p=word(head+4),prev=0,count=0;*matches=0;
 if((!order&&n!=28)||(order&&n!=129&&n!=132))return 0xffffffffu;
 while(p&&count<512){
  if(!span(p,n+8)||bytes_word(p+n)!=prev)return 0xffffffffu;
  if(order){if(own_name(p,128))++*matches;}
  else if(half(p)==0xca){
   ++*matches;
   if(!s.event_name||word(p+8)!=(u32)s.event_name||word(p+12)!=(u32)s.icon_name)return 0xffffffffu;
  }
  prev=p;p=bytes_word(p+n+4);++count;
 }
 if(p||word(head+8)!=prev)return 0xffffffffu;
 return count;
}
static u32 launcher_snapshot(u32 *info,u32 *order,u32 *mi,u32 *mo){
 *info=launcher_list(0x3c207640,0,mi);*order=launcher_list(0x3c207b40,1,mo);
 return *info!=0xffffffffu&&*order!=0xffffffffu&&*mi<=1&&*mo<=1;
}
