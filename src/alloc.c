#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <alloc.h>
#include <context.h>
#include <error.h>

void stucAllocSetCustom(StucAlloc *pAlloc, StucAlloc *pCustomAlloc) {
	STUC_ASSERT("", pAlloc && pCustomAlloc);
	if (!pCustomAlloc->pMalloc || !pCustomAlloc->pCalloc || !pCustomAlloc->pFree) {
		printf("Failed to set custom alloc. One or more functions were NULL");
		return;
	}
	*pAlloc = *pCustomAlloc;
}

void stucAllocSetDefault(StucAlloc *pAlloc) {
	STUC_ASSERT("", pAlloc);
	pAlloc->pMalloc = malloc;
	pAlloc->pCalloc = calloc;
	pAlloc->pFree = free;
	pAlloc->pRealloc = realloc;
}

StucResult stucLinAllocInit(const StucAlloc *pAlloc, void **ppHandle, I32 size, I32 initLen) {
	StucResult err = STUC_SUCCESS;
	STUC_ASSERT("", pAlloc && ppHandle);
	STUC_ASSERT("", size > 0 && initLen > 0);
	LinAllocState *pState = pAlloc->pCalloc(1, sizeof(LinAllocState));
	pState->alloc = *pAlloc;
	pState->blockArrSize = 1;
	pState->blockCount = 1;
	pState->blockSize = initLen;
	pState->typeSize = size;
	pState->ppBlockArr = pAlloc->pMalloc(pState->blockArrSize * sizeof(void *));
	pState->ppBlockArr[0] = pAlloc->pCalloc(initLen, size);
	*ppHandle = pState;
	STUC_CATCH(0, err, ;);
	return err;
}

static
StucResult incrementBlock(LinAllocState *pState) {
	StucResult err = STUC_SUCCESS;
	STUC_ASSERT(
		"",
		pState->blockIdx < pState->blockArrSize &&
		pState->blockIdx < pState->blockCount &&
		pState->blockCount <= pState->blockArrSize
	);
	pState->blockIdx++;
	pState->idx = 0;
	pState->blockSize *= 2;
	if (pState->blockIdx == pState->blockArrSize) {
		pState->blockArrSize *= 2;
		pState->ppBlockArr = pState->alloc.pRealloc(
			pState->ppBlockArr,
			pState->blockArrSize * sizeof(void *)
		);
	}
	else if (pState->blockIdx < pState->blockCount) {
		return err;
	}
	pState->ppBlockArr[pState->blockIdx] =
		pState->alloc.pCalloc(pState->blockSize, pState->typeSize);
	pState->blockCount++;
	STUC_CATCH(0, err, ;);
	return err;
}

StucResult stucLinAlloc(void *pHandle, void **ppData, I32 len) {
	StucResult err = STUC_SUCCESS;
	STUC_ASSERT("", pHandle && ppData);
	LinAllocState *pState = pHandle;
	if (pState->idx == pState->blockSize ||
	    pState->idx + len > pState->blockSize) {

		err = incrementBlock(pState);
		STUC_THROW_IFNOT(err, "", 0);
	}
	*ppData = (U8 *)pState->ppBlockArr[pState->blockIdx] + pState->idx * pState->typeSize;
	pState->idx += len;
	STUC_CATCH(0, err, ;);
	return err;
}

StucResult stucLinAllocClear(void *pHandle, bool setToZero) {
	StucResult err = STUC_SUCCESS;
	STUC_ASSERT("", pHandle);
	LinAllocState *pState = pHandle;
	STUC_ASSERT("", pState->ppBlockArr);
	if (!pState->blockIdx && !pState->idx) {
		return err;
	}
	I32 blockSize = pState->blockSize;
	for (I32 i = pState->blockIdx; i >= 0; --i) {
		if (setToZero) {
			I32 len = i == pState->blockIdx ? pState->idx : blockSize;
			memset(pState->ppBlockArr[i], 0, len * pState->typeSize);
		}
		if (i) {
			blockSize /= 2;
		}
	}
	pState->idx = 0;
	pState->blockIdx = 0;
	pState->blockSize = blockSize;
	STUC_CATCH(0, err, ;);
	return err;
}

StucResult stucLinAllocDestroy(void *pHandle) {
	StucResult err = STUC_SUCCESS;
	STUC_ASSERT("", pHandle);
	LinAllocState *pState = pHandle;
	STUC_ASSERT("", pState->ppBlockArr);
	STUC_ASSERT(
		"",
		pState->blockCount >= 0 && pState->blockCount <= pState->blockArrSize
	);
	for (I32 i = 0; i < pState->blockCount; ++i) {
		pState->alloc.pFree(pState->ppBlockArr[i]);
	}
	pState->alloc.pFree(pState->ppBlockArr);
	pState->ppBlockArr = NULL;
	STUC_CATCH(0, err, ;);
	return err;
}