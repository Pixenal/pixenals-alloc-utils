#pragma once
#include <stddef.h>
#include <UvStucco.h>
#include <Types.h>

typedef struct {
	StucAlloc alloc;
	void **ppBlockArr;
	I32 blockIdx;
	I32 blockCount;
	I32 blockArrSize;
	I32 idx;
	I32 blockSize;
	I32 typeSize;
} LinAllocState;

void stucAllocSetCustom(StucAlloc *pAlloc, StucAlloc *pCustomAlloc);
void stucAllocSetDefault(StucAlloc *pAlloc);

StucResult stucLinAllocInit(const StucAlloc *pAlloc, void **ppHandle, I32 size, I32 initLen);
StucResult stucLinAlloc(void *pHandle, void **ppData, I32 len);
StucResult stucLinAllocClear(void *pHandle, bool setToZero);
StucResult stucLinAllocDestroy(void *pHandle);