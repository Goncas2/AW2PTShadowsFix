#include "aw2_core.h"
#include "aw2_data.h"
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

const size_t Aw2BuildCount=sizeof(Aw2Builds)/sizeof(*Aw2Builds);

const AW2Build *Aw2SelectBuild(uint64_t size,const BYTE digest[32]) {
    for(size_t i=0;i<Aw2BuildCount;i++)
        if(size==Aw2Builds[i].file_size&&!memcmp(digest,Aw2Builds[i].hash,32))return Aw2Builds+i;
    return NULL;
}

const AW2Build *Aw2IdentifyFile(const wchar_t *path,char *error,size_t capacity) {
    const AW2Build *selected=NULL;
    const char *reason="REFUSED: cannot read game executable.";
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(file==INVALID_HANDLE_VALUE){snprintf(error,capacity,"%s",reason);return NULL;}
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    BYTE *buffer=NULL,digest[32];LARGE_INTEGER size;
    if(!GetFileSizeEx(file,&size))goto done;
    size_t i=0;
    for(;i<Aw2BuildCount;i++)if(size.QuadPart==Aw2Builds[i].file_size)break;
    if(i==Aw2BuildCount){reason="REFUSED: unsupported EXE size. Restore a pristine supported build.";goto done;}
    reason="REFUSED: cannot compute EXE SHA-256. No memory patches applied.";
    buffer=HeapAlloc(GetProcessHeap(),0,1024*1024);
    if(!buffer)goto done;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;
    if(BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)<0)goto done;
    for(;;){DWORD got=0;if(!ReadFile(file,buffer,1024*1024,&got,NULL))goto done;
        if(!got)break;
        if(BCryptHashData(hash,buffer,got,0)<0)goto done;}
    if(BCryptFinishHash(hash,digest,32,0)<0)goto done;
    selected=Aw2SelectBuild((uint64_t)size.QuadPart,digest);
    reason="REFUSED: EXE SHA-256 differs from supported pristine builds. No memory patches applied.";
done:
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    if(buffer)HeapFree(GetProcessHeap(),0,buffer);
    CloseHandle(file);
    if(capacity)snprintf(error,capacity,"%s",selected?"":reason);
    return selected;
}
