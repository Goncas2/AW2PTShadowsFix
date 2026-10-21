#include "aw2_core.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

static int fail(AW2State *s,const char *reason,DWORD rva) {
    snprintf(s->error,sizeof(s->error),"%s (RVA 0x%08lx, Windows %lu)",reason,rva,GetLastError());
    return 0;
}
static int displacement(BYTE *at,uintptr_t next,uintptr_t target) {
    int64_t delta=(int64_t)target-(int64_t)next;
    if(delta<INT32_MIN||delta>INT32_MAX)return 0;
    int32_t value=(int32_t)delta;memcpy(at,&value,4);return 1;
}
int Aw2Relocate(const AW2Build *b,BYTE *output,uintptr_t code,uintptr_t image) {
    if(code<image||code-image>0x70000000u)return 0;
    memcpy(output,b->payload,sizeof(b->payload));
    for(size_t i=0;i<sizeof(b->relocations)/sizeof(*b->relocations);i++) {
        const AW2Relocation *r=b->relocations+i;
        if(!displacement(output+r->displacement,code+r->next,image+r->target))return 0;
    }
    return 1;
}
static BYTE *allocate_near(BYTE *image,const AW2Build *b) {
    SYSTEM_INFO sys;GetSystemInfo(&sys);
    uintptr_t gran=sys.dwAllocationGranularity;
    uintptr_t cursor=((uintptr_t)image+b->image_size+gran-1)&~(gran-1);
    uintptr_t limit=(uintptr_t)image+0x70000000u;
    while(cursor+0x1000<limit) {
        MEMORY_BASIC_INFORMATION m;
        if(!VirtualQuery((void*)cursor,&m,sizeof(m)))return NULL;
        if(m.State==MEM_FREE) {
            BYTE *p=VirtualAlloc((void*)cursor,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(p)return p;
        }
        uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;
        if(next<=cursor)return NULL;
        cursor=(next+gran-1)&~(gran-1);
    }
    return NULL;
}
void Aw2DiscardUnpublished(AW2State *s) {
    if(s->active)return; /* Never free code reachable from installed game jumps. */
    if(s->registered){RtlDeleteFunctionTable(s->table);s->registered=0;}
    if(s->code){VirtualFree(s->code,0,MEM_RELEASE);s->code=NULL;}
}
static int prepare(AW2State *s,BYTE *image) {
    const AW2Build *b=s->build;
    s->image=image;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)image;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||(DWORD)dos->e_lfanew!=b->pe_offset)return fail(s,"Unsupported DOS header",0);
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64*)(image+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.TimeDateStamp!=b->timestamp||
       nt->OptionalHeader.SizeOfImage!=b->image_size)return fail(s,"Unsupported PE header",0);
    for(size_t i=0;i<8;i++) {
        const AW2PatchSpec *p=b->patches+i;
        if(memcmp(image+p->rva,p->before,p->length))return fail(s,"Conflicting or already patched instructions",p->rva);
        memcpy(s->replacements[i],p->after,p->length);
    }
    /* Preserve the two rejected scheduling edits and validate parent unwind layout. */
    if(memcmp(image+b->scheduling_rva,b->scheduling_before,6)||
       memcmp(image+b->scheduling_control,"\x01\x01",2))return fail(s,"Conflicting scheduling edits",b->scheduling_rva);
    DWORD64 parent_base=0;
    PRUNTIME_FUNCTION parent=RtlLookupFunctionEntry((DWORD64)(uintptr_t)image+b->relocations[3].target,&parent_base,NULL);
    if(!parent||parent_base!=(DWORD64)(uintptr_t)image||parent->BeginAddress!=b->parent.BeginAddress||
       parent->EndAddress!=b->parent.EndAddress||parent->UnwindData!=b->parent.UnwindData)
        return fail(s,"Unexpected parent unwind layout",b->relocations[3].target);
    s->code=allocate_near(image,b);
    if(!s->code)return fail(s,"Cannot allocate nearby runtime code",0);
    if(!Aw2Relocate(b,s->code,(uintptr_t)s->code,(uintptr_t)image))return fail(s,"Relative address out of range",0);
    for(size_t i=0;i<8;i++) {
        const AW2PatchSpec *p=b->patches+i;
        if(p->helper<0)continue;
        memset(s->replacements[i],0x90,p->length);s->replacements[i][0]=0xe9;
        if(!displacement(s->replacements[i]+1,(uintptr_t)image+p->rva+5,(uintptr_t)s->code+p->helper))
            return fail(s,"Hook jump out of range",p->rva);
    }
    DWORD rva=(DWORD)((uintptr_t)s->code-(uintptr_t)image);
    s->table=(PRUNTIME_FUNCTION)(s->code+512);
    s->table[0]=(RUNTIME_FUNCTION){rva,rva+319,rva+332};
    s->table[1]=(RUNTIME_FUNCTION){rva+352,rva+480,rva+480};
    DWORD old;
    if(!VirtualProtect(s->code,0x1000,PAGE_EXECUTE_READ,&old))return fail(s,"Cannot protect runtime code",rva);
    if(!FlushInstructionCache(GetCurrentProcess(),s->code,496))return fail(s,"Cannot flush runtime code",rva);
    if(!RtlAddFunctionTable(s->table,2,(DWORD64)(uintptr_t)image))return fail(s,"Cannot register runtime unwind table",rva);
    s->registered=1;
    DWORD64 found_base=0;
    PRUNTIME_FUNCTION found=RtlLookupFunctionEntry((DWORD64)(uintptr_t)s->code+352,&found_base,NULL);
    if(!found||found_base!=(DWORD64)(uintptr_t)image||found->BeginAddress!=rva+352)
        return fail(s,"Runtime unwind table lookup failed",rva+352);
    return 1;
}
int Aw2Install(AW2State *s,BYTE *image,const AW2Build *b) {
    /* Only immutable, reviewed profiles from the compiled table are accepted. */
    size_t selected=0;
    while(selected<Aw2BuildCount&&b!=Aw2Builds+selected)selected++;
    if(selected==Aw2BuildCount)return fail(s,"Unknown build profile",0);
    s->build=b;
    if(!prepare(s,image)){Aw2DiscardUnpublished(s);return 0;}
    size_t protected_count=0;
    for(;protected_count<8;protected_count++) {
        const AW2PatchSpec *p=b->patches+protected_count;
        if(!VirtualProtect(image+p->rva,p->length,PAGE_EXECUTE_READWRITE,s->old_protection+protected_count))break;
    }
    if(protected_count!=8) {
        fail(s,"Cannot make all patch sites writable",b->patches[protected_count].rva);
        while(protected_count){DWORD ignored;--protected_count;const AW2PatchSpec *p=b->patches+protected_count;
            VirtualProtect(image+p->rva,p->length,s->old_protection[protected_count],&ignored);}
        Aw2DiscardUnpublished(s);return 0;
    }
    /* All validation/allocation/unwind work completes before publishing any hook.
       Called at loader startup only, never as a hot-attach or hot-toggle API. */
    for(size_t i=0;i<8;i++)memcpy(image+b->patches[i].rva,s->replacements[i],b->patches[i].length);
    if(!FlushInstructionCache(GetCurrentProcess(),image,b->image_size)) {
        for(size_t i=0;i<8;i++)memcpy(image+b->patches[i].rva,b->patches[i].before,b->patches[i].length);
        FlushInstructionCache(GetCurrentProcess(),image,b->image_size);
        fail(s,"Instruction cache flush failed; reverted hooks",0);
    } else s->active=1;
    /* Some sites share a page: restore protections in reverse acquisition order. */
    for(size_t i=8;i>0;i--){DWORD ignored;const AW2PatchSpec *p=b->patches+i-1;
        if(!VirtualProtect(image+p->rva,p->length,s->old_protection[i-1],&ignored))s->protection_warning=1;}
    if(!s->active)Aw2DiscardUnpublished(s);
    return s->active;
}
