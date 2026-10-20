#ifndef AW2_CORE_H
#define AW2_CORE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { DWORD displacement,next,target; } AW2Relocation;
typedef struct { DWORD rva; unsigned length; int helper; BYTE before[24],after[24]; } AW2PatchSpec;
typedef struct {
    BYTE *image,*code;
    PRUNTIME_FUNCTION table;
    BYTE replacements[8][24];
    DWORD old_protection[8];
    int registered,active,protection_warning;
    char error[256];
} AW2State;
int Aw2Relocate(BYTE *output,uintptr_t code,uintptr_t image);
int Aw2Install(AW2State *state,BYTE *image);
void Aw2DiscardUnpublished(AW2State *state);
#endif
