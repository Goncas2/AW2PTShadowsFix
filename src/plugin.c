#include "aw2_core.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static HMODULE self;
static volatile LONG initialized;
static AW2State runtime;
static HANDLE logfile=INVALID_HANDLE_VALUE;
static void log_line(const char *text) {
    if(logfile==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME t;GetLocalTime(&t);
    char line[1024];DWORD done;
    int n=snprintf(line,sizeof(line),"[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,text);
    if(n>0)WriteFile(logfile,line,(DWORD)((size_t)n<sizeof(line)?(size_t)n:sizeof(line)-1),&done,NULL);
    FlushFileBuffers(logfile);
}
static void open_log(void) {
    wchar_t path[32768];DWORD n=GetModuleFileNameW(self,path,32768);
    if(!n||n>=32768)return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;
    wcscpy(slash+1,L"AW2PTShadowsFix.log");
    logfile=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
}
__declspec(dllexport) void InitializeASI(void) {
    if(InterlockedCompareExchange(&initialized,1,0))return;
    open_log();log_line("AW2PTShadowsFix 0.1.2: initializing via InitializeASI.");
    wchar_t path[32768];DWORD n=GetModuleFileNameW(NULL,path,32768);
    if(!n||n>=32768) {
        log_line("REFUSED: cannot determine host executable path.");goto done;
    }
    char error[256];
    const AW2Build *build=Aw2IdentifyFile(path,error,sizeof(error));
    if(!build){log_line(error);goto done;}
    char selected[128];
    snprintf(selected,sizeof(selected),"Verified EXE profile: %s",build->name);
    log_line(selected);
    /* Keep our code and state alive as long as game jumps can reach them. */
    HMODULE pinned=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          (LPCWSTR)(uintptr_t)&InitializeASI,&pinned)) {
        log_line("REFUSED: cannot pin plugin lifetime.");goto done;
    }
    if(!Aw2Install(&runtime,(BYTE*)GetModuleHandleW(NULL),build)) {
        log_line(runtime.error);log_line("INACTIVE: validation/preparation failed; no fix installed.");goto done;
    }
    char detail[512];
    snprintf(detail,sizeof(detail),"ACTIVE: OMM 1+2+3, grass/mixed-LOD registration+cleanup+notifications; image=%p runtime=%p; corrected dynamic unwind registered.",runtime.image,runtime.code);
    log_line(detail);
    log_line("Disk EXE unchanged. Scheduling edits excluded. Restart required for install/removal; no hot toggle.");
    if(runtime.protection_warning)log_line("WARNING: a patch page protection could not be restored.");
done:
    if(logfile!=INVALID_HANDLE_VALUE){CloseHandle(logfile);logfile=INVALID_HANDLE_VALUE;}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);}
    /* InitializeASI performs work outside the loader lock. No hot unloading. */
    return TRUE;
}
