/* Local compiler helpers, no libc/ROM imports. */
typedef unsigned int u32;
void *memset(void *p,int c,u32 n) { unsigned char *d=p; for(u32 i=0;i<n;i++) d[i]=(unsigned char)c; return p; }
void *memcpy(void *p,const void *s,u32 n) { unsigned char *d=p; const unsigned char *q=s; for(u32 i=0;i<n;i++)d[i]=q[i]; return p; }
void __aeabi_memclr(void *p,u32 n) { memset(p,0,n); }
void __aeabi_memclr4(void *p,u32 n) { memset(p,0,n); }
void __aeabi_memcpy(void *p,const void *s,u32 n) { memcpy(p,s,n); }
void __aeabi_memcpy4(void *p,const void *s,u32 n) { memcpy(p,s,n); }
