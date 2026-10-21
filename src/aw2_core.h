#ifndef AW2_CORE_H
#define AW2_CORE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { DWORD displacement,next,target; } AW2Relocation;
typedef struct { DWORD rva; unsigned length; int helper; BYTE before[24],after[24]; } AW2PatchSpec;
typedef struct {
    const char *name;
    DWORD file_size,timestamp,pe_offset,image_size;
    BYTE hash[32],payload[496];
    AW2Relocation relocations[5];
    AW2PatchSpec patches[8];
    DWORD scheduling_rva;
    BYTE scheduling_before[6];
    DWORD scheduling_control;
    RUNTIME_FUNCTION parent;
} AW2Build;
extern const AW2Build Aw2Builds[];
extern const size_t Aw2BuildCount;
const AW2Build *Aw2SelectBuild(uint64_t size,const BYTE digest[32]);
const AW2Build *Aw2IdentifyFile(const wchar_t *path,char *error,size_t capacity);

typedef struct {
    const AW2Build *build;
    BYTE *image,*code;
    PRUNTIME_FUNCTION table;
    BYTE replacements[8][24];
    DWORD old_protection[8];
    int registered,active,protection_warning;
    char error[256];
} AW2State;
int Aw2Relocate(const AW2Build *build,BYTE *output,uintptr_t code,uintptr_t image);
int Aw2Install(AW2State *state,BYTE *image,const AW2Build *build);
void Aw2DiscardUnpublished(AW2State *state);
#endif
