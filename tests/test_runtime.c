#include "../src/aw2_core.h"
#include "../src/aw2_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s Windows=%lu\n",__LINE__,#x,GetLastError());exit(2);}}while(0)
extern unsigned Aw2RunNotification(void*,void*,void*,void*,unsigned,unsigned);
static unsigned checks,registration_cases,notification_cases,unwind_cases;
static void *captured[16],*captured_manager;
static unsigned captured_count,captured_id,calls;
static void capture_static(void *manager,unsigned id,void **handles,unsigned count) {
    REQUIRE(count<=16);captured_manager=manager;captured_id=id;captured_count=count;calls++;
    memcpy(captured,handles,count*sizeof(void*));
}
static void putptr(BYTE *p,size_t at,const void *v){memcpy(p+at,&v,8);}
static void put32(BYTE *p,size_t at,DWORD v){memcpy(p+at,&v,4);}
static DWORD get32(BYTE *p,size_t at){DWORD v;memcpy(&v,p+at,4);return v;}
static void make_stub(BYTE *at,void *target) {
    DWORD old;REQUIRE(VirtualProtect(at,16,PAGE_EXECUTE_READWRITE,&old));
    at[0]=0x48;at[1]=0xb8;memcpy(at+2,&target,8);at[10]=0xff;at[11]=0xe0;
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),at,16));
}
static void make_ret(BYTE *at) {
    DWORD old;REQUIRE(VirtualProtect(at,1,PAGE_EXECUTE_READWRITE,&old));*at=0xc3;
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),at,1));
}
static const DWORD64 saved[8]={0x1111111122222222ULL,0x3333333344444444ULL,0x5555555566666666ULL,0x7777777788888888ULL,0x99999999aaaaaaaaULL,0xbbbbbbbbccccccccULL,0xddddddddeeeeeeeeULL,0x123456789abcdef0ULL};
static void unwind_helper(BYTE *image,AW2State *s) {
    BYTE *stack=VirtualAlloc(NULL,0x3000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);REQUIRE(stack);
    DWORD64 entry=(DWORD64)(uintptr_t)(stack+0x1808),ret=0x1020304050607080ULL;
    *(DWORD64*)(uintptr_t)entry=ret;
    int offsets[]={8,16,24,32,0x38,40,48,0x40};
    for(int i=0;i<8;i++)*(DWORD64*)(uintptr_t)(entry-offsets[i])=saved[i];
    unsigned pcs[]={352,355,362,364,368,372,376,383,391,393,396,398,401,406,411,413,416,420,427,434,440,442,445,447,449,452,455,460,464,467,469,477};
    for(int vis=0;vis<2;vis++)for(unsigned i=0;i<32;i++) {
        CONTEXT c;memset(&c,0,sizeof(c));c.ContextFlags=CONTEXT_FULL;c.Rsp=entry-0x88;
        c.Rbx=c.Rbp=c.Rsi=c.Rdi=c.R12=c.R13=c.R14=c.R15=0xfefefefefefefefeULL;
        if(vis)c.R12=saved[4];
        c.Rip=(DWORD64)(uintptr_t)s->code+pcs[i];
        DWORD64 base=0,est=0;PVOID handler=NULL;
        PRUNTIME_FUNCTION f=RtlLookupFunctionEntry(c.Rip,&base,NULL);
        REQUIRE(f&&base==(DWORD64)(uintptr_t)image);
        RtlVirtualUnwind(0,base,c.Rip,f,&c,&handler,&est,NULL);
        REQUIRE(c.Rip==ret&&c.Rsp==entry+8&&c.Rbx==saved[0]&&c.Rbp==saved[1]&&c.Rsi==saved[2]&&c.Rdi==saved[3]&&c.R12==saved[4]&&c.R13==saved[5]&&c.R14==saved[6]&&c.R15==saved[7]);
        unwind_cases++;
    }
    struct Point{DWORD offset;int stage;}points[]={{0,0},{1,1},{8,2},{11,2},{16,2},{55,2},{105,2},{186,2},{298,2},{310,2},{317,1},{318,0}};
    const DWORD64 token=0x123456789abcdef0ULL;
    *(DWORD64*)(uintptr_t)(entry-8)=token;
    for(unsigned i=0;i<12;i++) {
        CONTEXT c;memset(&c,0,sizeof(c));c.ContextFlags=CONTEXT_FULL;
        c.Rip=(DWORD64)(uintptr_t)s->code+points[i].offset;
        c.Rsp=entry-(points[i].stage>=1?8:0)-(points[i].stage>=2?0xa0:0);
        c.Rbx=points[i].offset>=11&&points[i].offset<318?0x1111222233334444ULL:token;
        DWORD64 base=0,est=0;PVOID handler=NULL;PRUNTIME_FUNCTION f=RtlLookupFunctionEntry(c.Rip,&base,NULL);
        REQUIRE(f&&base==(DWORD64)(uintptr_t)image);RtlVirtualUnwind(0,base,c.Rip,f,&c,&handler,&est,NULL);
        REQUIRE(c.Rip==ret&&c.Rsp==entry+8&&c.Rbx==token);unwind_cases++;
    }
    struct Point clean[]={{0x28,2},{0x2b,2},{0x2e,2},{0x5c,2},{0x60,1},{0x61,0}};
    for(unsigned i=0;i<6;i++) {
        CONTEXT c;memset(&c,0,sizeof(c));c.ContextFlags=CONTEXT_FULL;
        c.Rip=(DWORD64)(uintptr_t)image+0x2304050+clean[i].offset;
        c.Rsp=entry-(clean[i].stage>=1?8:0)-(clean[i].stage>=2?0x20:0);
        c.Rbx=clean[i].stage==0?token:0x1111222233334444ULL;
        DWORD64 base=0,est=0;PVOID handler=NULL;PRUNTIME_FUNCTION f=RtlLookupFunctionEntry(c.Rip,&base,NULL);
        REQUIRE(f);RtlVirtualUnwind(0,base,c.Rip,f,&c,&handler,&est,NULL);
        REQUIRE(c.Rip==ret&&c.Rsp==entry+8&&c.Rbx==token);unwind_cases++;
    }
    VirtualFree(stack,0,MEM_RELEASE);
}
static void test_registration(BYTE *image,AW2State *s) {
    make_ret(image+0x23040c5);make_stub(image+0x22aab60,(void*)(uintptr_t)&capture_static);
    putptr(image,0x397ff60,(void*)0x1122334455667788ULL);
    BYTE object[0x100],mesh[0x500];
    void *opaque[24],*transparent[24],*overrides[16];
    for(unsigned i=0;i<24;i++){opaque[i]=(void*)(uintptr_t)(0x1000+i*16);transparent[i]=(void*)(uintptr_t)(0x2000+i*16);}
    for(unsigned i=0;i<16;i++)overrides[i]=(void*)(uintptr_t)(0x3000+i*16);
    for(unsigned lods=1;lods<=8;lods++)for(unsigned defs=1;defs<=8;defs++)for(unsigned mode=0;mode<3;mode++) {
        memset(object,0,sizeof(object));memset(mesh,0,sizeof(mesh));calls=0;captured_count=0;
        putptr(object,8,mesh);put32(object,0x48,7);put32(object,0x4c,0x800000|(mode==1?0x1000000:0));
        mesh[0x270]=(BYTE)lods;mesh[0x272]=(BYTE)defs;put32(mesh,0x178,1);
        putptr(mesh,0x380,opaque);putptr(mesh,0x390,transparent);
        if(mode==2)putptr(object,0x18,overrides);
        for(unsigned i=0;i<defs;i++)put32(mesh,0x1c8+i*4,i%3==0?1:0);
        void *expected[16];unsigned count=0;
        for(unsigned i=0;i<lods;i++)if(get32(mesh,0x1c8+(i<defs?i:defs-1)*4)==0) {
            expected[count++]=mode==2?overrides[i*2]:opaque[(mode==1?lods:0)+i];
            expected[count++]=mode==2?overrides[i*2+1]:transparent[(mode==1?lods:0)+i];
        }
        ((void(*)(void*))(uintptr_t)s->code)(object);
        REQUIRE(calls==(count?1u:0u));
        REQUIRE((get32(object,0x4c)&0x400000)==(count?0x400000u:0));
        if(count){REQUIRE(captured_count==count&&captured_id==7&&captured_manager==(void*)0x1122334455667788ULL);REQUIRE(!memcmp(expected,captured,count*8));}
        registration_cases++;
    }
    for(unsigned mode=0;mode<6;mode++) {
        memset(object,0,sizeof(object));memset(mesh,0,sizeof(mesh));calls=0;
        putptr(object,8,mesh);put32(object,0x4c,mode==0?0:mode==1?0xc00000:0x800000);
        mesh[0x270]=mode==2?0:mode==3?9:2;mesh[0x272]=mode==4?0:mode==5?9:2;
        ((void(*)(void*))(uintptr_t)s->code)(object);REQUIRE(!calls);registration_cases++;
    }
}
static void test_notifications(BYTE *image,AW2State *s) {
    make_ret(image+0x225c633);make_ret(image+0x225c763);
    BYTE object[0x100],manager[0x200],builder[0x300];void *objects[4]={0,0,0,object};
    DWORD ids[64];DWORD64 bitmap[16];
    for(unsigned vis=0;vis<2;vis++)for(unsigned rt=0;rt<2;rt++)for(unsigned mode=0;mode<4;mode++)for(unsigned count=0;count<=16;count++) {
        memset(object,0,sizeof(object));memset(manager,0,sizeof(manager));memset(builder,0,sizeof(builder));memset(bitmap,0,sizeof(bitmap));memset(ids,0xff,sizeof(ids));
        DWORD flags[]={0,0x400000,0x800000,0xc00000};put32(object,0x4c,flags[mode]);put32(object,0x48,2);
        putptr(manager,0x18,objects);putptr(manager,0x160,ids);putptr(builder,0x228,bitmap);
        for(unsigned i=0;i<count;i++)ids[32+i]=63+i*7;
        DWORD64 expected[16]={0};if(rt&&mode==3)for(unsigned i=0;i<count;i++)expected[ids[32+i]/64]|=1ULL<<(ids[32+i]%64);
        REQUIRE(Aw2RunNotification(s->code+(vis?364:352),object,manager,builder,vis,rt)==0);
        REQUIRE(!memcmp(bitmap,expected,sizeof(bitmap)));notification_cases++;
    }
}
static BYTE *make_image(const BYTE *raw,unsigned pass) {
    void *hint=(void*)(uintptr_t)(0x200000000ULL+pass*0x300000000ULL);
    BYTE *image=VirtualAlloc(hint,AW2_IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);REQUIRE(image==hint);
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64*)(raw+AW2_PE_OFFSET);
    memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)memcpy(image+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
    return image;
}
int main(int argc,char **argv) {
    REQUIRE(argc==2);FILE *f=fopen(argv[1],"rb");REQUIRE(f);BYTE *raw=malloc(AW2_SOURCE_SIZE);REQUIRE(raw);
    REQUIRE(fread(raw,1,AW2_SOURCE_SIZE,f)==AW2_SOURCE_SIZE);REQUIRE(fgetc(f)==EOF);fclose(f);
    for(unsigned pass=0;pass<2;pass++) {
        BYTE *image=make_image(raw,pass),*baseline=malloc(AW2_IMAGE_SIZE);REQUIRE(baseline);memcpy(baseline,image,AW2_IMAGE_SIZE);
        IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64*)(image+AW2_PE_OFFSET);
        IMAGE_DATA_DIRECTORY dir=nt->OptionalHeader.DataDirectory[3];PRUNTIME_FUNCTION original=(PRUNTIME_FUNCTION)(image+dir.VirtualAddress);
        REQUIRE(RtlAddFunctionTable(original,dir.Size/12,(DWORD64)(uintptr_t)image));
        /* A conflict must refuse the whole fix and leave all other bytes alone. */
        image[aw2_specs[7].rva]^=1;AW2State rejected={0};REQUIRE(!Aw2Install(&rejected,image));
        REQUIRE(!rejected.code&&!rejected.active);image[aw2_specs[7].rva]^=1;
        REQUIRE(!memcmp(image,baseline,AW2_IMAGE_SIZE));checks++;
        AW2State s={0};REQUIRE(Aw2Install(&s,image));REQUIRE(s.active&&!s.protection_warning);
        for(size_t i=0;i<8;i++){memcpy(baseline+aw2_specs[i].rva,s.replacements[i],aw2_specs[i].length);}
        REQUIRE(!memcmp(image,baseline,AW2_IMAGE_SIZE));checks++;
        /* Relocations are exactly the five references, plus runtime table metadata. */
        BYTE reconstructed[496];REQUIRE(Aw2Relocate(reconstructed,(uintptr_t)s.code,(uintptr_t)image));REQUIRE(!memcmp(reconstructed,s.code,496));
        REQUIRE(!Aw2Relocate(reconstructed,(uintptr_t)image+0x90000000ULL,(uintptr_t)image));checks++;
        unwind_helper(image,&s);test_registration(image,&s);test_notifications(image,&s);
        REQUIRE(RtlDeleteFunctionTable(s.table));REQUIRE(RtlDeleteFunctionTable(original));
        VirtualFree(s.code,0,MEM_RELEASE);VirtualFree(image,0,MEM_RELEASE);free(baseline);
        printf("PASS pass=%u: relocated native payload, transactional byte audit, ABI/behavior and real Windows unwind\n",pass);
    }
    free(raw);
    printf("PASS TOTAL: %u transaction/range checks; %u registration cases; %u notification cases; %u native unwind cases. No game code or live process executed/modified.\n",checks,registration_cases,notification_cases,unwind_cases);
    return 0;
}
