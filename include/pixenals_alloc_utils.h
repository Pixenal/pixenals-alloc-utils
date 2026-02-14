/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stddef.h>
#include <string.h>

#include "../../pixenals-types/include/pixenals_types.h"
#include "../../pixenals-error-utils/include/pixenals_error_utils.h"

typedef uint8_t U8;
typedef int32_t I32;

typedef struct PixalcFPtrs {
	void *(*fpMalloc)(size_t);
	void *(*fpCalloc)(size_t, size_t);
	void (*fpFree)(void *);
	void *(*fpRealloc)(void *, size_t);
} PixalcFPtrs;

typedef struct PixalcLinAllocBlock {
	void *pData;
	I32 size;
	I32 count;
	I32 lessThan;
} PixalcLinAllocBlock;

typedef struct PixalcRegion {
	I32 idx;
	I32 len;
} PixalcRegion;

typedef struct PixalcRegionArr {
	PixalcRegion *pArr;
	I32 size;
	I32 count;
} PixalcRegionArr;

typedef struct PixalcLinAlloc {
	PixalcLinAllocBlock *pBlockArr;
	PixalcRegionArr freed;
	PixalcFPtrs alloc;
	I32 blockIdx;
	I32 blockCount;
	I32 blockArrSize;
	I32 typeSize;
	I32 linIdx;
	bool zeroOnClear;
	bool valid;
} PixalcLinAlloc;

typedef struct PixalcLinAllocArr {
	PixalcLinAlloc *pArr;
	I32 size;
	I32 count;
} PixalcLinAllocArr;

typedef struct PixalcLinAllocIter {
	const PixalcLinAlloc *pState;
	PixtyRange range;
	I32 rangeSize;
	I32 count;
	I32 block;
	I32 idx;
} PixalcLinAllocIter;

#define PIXALC_DYN_ARR_RESIZE(t, pAlloc, pDynArr, newSize)\
	PIX_ERR_ASSERT("", newSize > 0);\
	if (!(pDynArr)->size) {\
		PIX_ERR_ASSERT("", !(pDynArr)->pArr);\
		(pDynArr)->size = newSize;\
		(pDynArr)->pArr = (pAlloc)->fpMalloc((pDynArr)->size * sizeof(t));\
	}\
	else if (newSize >= (pDynArr)->size) {\
		(pDynArr)->size *= 2;\
		if (newSize > (pDynArr)->size) {\
			(pDynArr)->size = newSize;\
		}\
		(pDynArr)->pArr =\
			(pAlloc)->fpRealloc((pDynArr)->pArr, (pDynArr)->size * sizeof(t));\
	}

#define PIXALC_DYN_ARR_ADD_ALT(tSize, pAlloc, pDynArr, newIdx)\
	PIX_ERR_ASSERT("", (pDynArr)->count <= (pDynArr)->size);\
	if (!(pDynArr)->size) {\
		PIX_ERR_ASSERT("", !(pDynArr)->pArr);\
		(pDynArr)->size = 4;\
		(pDynArr)->pArr = (pAlloc)->fpMalloc((pDynArr)->size * tSize);\
	}\
	else if ((pDynArr)->count == (pDynArr)->size) {\
		(pDynArr)->size *= 2;\
		(pDynArr)->pArr =\
			(pAlloc)->fpRealloc((pDynArr)->pArr, (pDynArr)->size * tSize);\
	}\
	newIdx = (pDynArr)->count;\
	(pDynArr)->count++;

#define PIXALC_DYN_ARR_ADD(t, pAlloc, pDynArr, newIdx)\
	PIXALC_DYN_ARR_ADD_ALT(sizeof(t), pAlloc, pDynArr, newIdx);

static inline
void pixalcLinAllocInit(
	const PixalcFPtrs *pAlloc,
	PixalcLinAlloc *pHandle,
	I32 size,
	I32 initLen,
	bool zeroOnClear
) {
	PIX_ERR_ASSERT("", pAlloc && pHandle);
	PIX_ERR_ASSERT("", size > 0 && initLen > 0);
	*pHandle = (PixalcLinAlloc){
		.alloc = *pAlloc,
		.blockArrSize = 1,
		.blockCount = 1,
		.zeroOnClear = zeroOnClear,
		.typeSize = size,
		.valid = true
	};
	pHandle->pBlockArr = pAlloc->fpMalloc(pHandle->blockArrSize * sizeof(PixalcLinAllocBlock));
	pHandle->pBlockArr[0] = (PixalcLinAllocBlock) {
		.size = initLen,
		.pData = pAlloc->fpCalloc(initLen, size)
	};
}

I32 pixalcLinAllocCheckForFreed(PixalcLinAlloc *pHandle, void **ppData, I32 len);

static inline
void pixalcLinAllocBlockInc(PixalcLinAlloc *pHandle, I32 requiredLen) {
	PIX_ERR_ASSERT(
		"",
		requiredLen > 0 &&
		pHandle->blockIdx >= 0 &&
		pHandle->blockIdx < pHandle->blockArrSize &&
		pHandle->blockIdx < pHandle->blockCount &&
		pHandle->blockCount <= pHandle->blockArrSize
	);
	bool incd = false;
	if (pHandle->pBlockArr[pHandle->blockIdx].count) {
		//current block contains data, so increment
		pHandle->pBlockArr[pHandle->blockIdx].lessThan = pHandle->linIdx;
		pHandle->blockIdx++;
		incd = true;
	}
	if (pHandle->blockIdx == pHandle->blockArrSize) {
		pHandle->blockArrSize *= 2;
		pHandle->pBlockArr = pHandle->alloc.fpRealloc(
			pHandle->pBlockArr,
			pHandle->blockArrSize * sizeof(PixalcLinAllocBlock)
		);
	}
	else if (pHandle->blockIdx != pHandle->blockCount) {
		//this block was already alloc'ed (first block, or blocks were cleared)
		PIX_ERR_ASSERT("invalid state", !(!pHandle->linIdx ^ !pHandle->blockIdx));
		PixalcLinAllocBlock *pBlock = pHandle->pBlockArr + pHandle->blockIdx;
		I32 prevSize = pHandle->blockIdx ?
			pHandle->pBlockArr[pHandle->blockIdx - 1].size : 0;
		bool zero = false;
		if (pBlock->size < prevSize + requiredLen) {
			pBlock->size = prevSize + requiredLen;
			pBlock->pData = pHandle->alloc.fpRealloc(
				pBlock->pData,
				pHandle->typeSize * pBlock->size
			);
			zero = true;
		}
		else {
			PIX_ERR_ASSERT(
				"didn't inc block and didn't realloc?",
				incd
			);
		}
		//block[0] is already memset on clear
		if (zero || pHandle->zeroOnClear && pHandle->blockIdx) {
			memset(pBlock->pData, 0, pHandle->typeSize * pBlock->size);
		}
		return; 
	}
	PixalcLinAllocBlock *pNewBlock = pHandle->pBlockArr + pHandle->blockIdx;
	I32 prevSize = pHandle->pBlockArr[pHandle->blockIdx - 1].size;
	*pNewBlock = (PixalcLinAllocBlock) {.size = (prevSize + requiredLen) * 2};
	pNewBlock->pData = pHandle->alloc.fpCalloc(pNewBlock->size, pHandle->typeSize);
	pHandle->blockCount++;
}

//if len > 1, the returned array will be contiguous
static inline
I32 pixalcLinAlloc(PixalcLinAlloc *pHandle, void **ppData, I32 len) {
	PIX_ERR_ASSERT("", pHandle && ppData);
	*ppData = NULL;
	I32 retIdx = pixalcLinAllocCheckForFreed(pHandle, ppData, len);
	PIX_ERR_ASSERT("", !(retIdx != -1 ^ !!*ppData))
	if (*ppData) {
		return retIdx;
	}
	PixalcLinAllocBlock *pBlock = pHandle->pBlockArr + pHandle->blockIdx;
	if (pBlock->count == pBlock->size || pBlock->count + len > pBlock->size) {
		pixalcLinAllocBlockInc(pHandle, len);
		pBlock = pHandle->pBlockArr + pHandle->blockIdx;
	}
	*ppData = (U8 *)pBlock->pData + pBlock->count * pHandle->typeSize;
	retIdx = pHandle->linIdx;
	pBlock->count += len;
	pHandle->linIdx += len;
	return retIdx;
}

void pixalcLinAllocClear(PixalcLinAlloc *pHandle);

static inline
void pixalcLinAllocDestroy(PixalcLinAlloc *pHandle) {
	PIX_ERR_ASSERT("", pHandle);
	PIX_ERR_ASSERT("", pHandle->pBlockArr);
	PIX_ERR_ASSERT(
		"",
		pHandle->blockCount >= 0 && pHandle->blockCount <= pHandle->blockArrSize
	);
	for (I32 i = 0; i < pHandle->blockCount; ++i) {
		pHandle->alloc.fpFree(pHandle->pBlockArr[i].pData);
	}
	if (pHandle->freed.pArr) {
		pHandle->alloc.fpFree(pHandle->freed.pArr);
	}
	pHandle->alloc.fpFree(pHandle->pBlockArr);
	*pHandle = (PixalcLinAlloc) {0};
}

void *pixalcLinAllocIdx(PixalcLinAlloc *pHandle, I32 idx);
const void *pixalcLinAllocIdxConst(const PixalcLinAlloc *pState, I32 idx);

static inline
I32 pixalcLinAllocGetCount(const PixalcLinAlloc *pHandle) {
	PIX_ERR_ASSERT(
		"",
		pHandle->valid && pHandle->pBlockArr != NULL
	);
	I32 total = 0;
	for (I32 i = 0; i <= pHandle->blockIdx; ++i) {
		total += pHandle->pBlockArr[i].count;
	}
	return total;
}

//note, freed regions will be included in iteration.
//so if pixalcLinAllocRegionFree has been called prior, make sure to check validity when
//iterating (regions are zero'd on clear).
void pixalcLinAllocIterInit(PixalcLinAlloc *pState, PixtyRange range, PixalcLinAllocIter *pIter);

void pixalcLinAllocRegionClear(PixalcLinAlloc *pState, void *pStart, I32 len);

static inline
bool pixalcLinAllocIterAtEnd(const PixalcLinAllocIter *pIter) {
	return
		pIter->count >= pIter->rangeSize ||
		pIter->block > pIter->pState->blockIdx ||
		!pIter->pState->blockCount ||
		pIter->idx >= pIter->pState->pBlockArr[pIter->block].count;
}

static inline
void pixalcLinAllocIterInc(PixalcLinAllocIter *pIter) {
	PIX_ERR_ASSERT(
		"",
		pIter->block <= pIter->pState->blockIdx &&
		pIter->idx < pIter->pState->pBlockArr[pIter->block].count
	);
	const PixalcLinAllocBlock *pBlock = pIter->pState->pBlockArr + pIter->block;
	pIter->idx++;
	if (pIter->idx == pBlock->count) {
		pIter->block++;
		pIter->idx = 0;
	}
	pIter->count++;
}

static inline
void *pixalcLinAllocGetItem(const PixalcLinAllocIter *pIter) {
	PIX_ERR_ASSERT(
		"",
		pIter->block <= pIter->pState->blockIdx &&
		pIter->idx < pIter->pState->pBlockArr[pIter->block].count
	);
	const PixalcLinAllocBlock *pBlock = pIter->pState->pBlockArr + pIter->block;
	return (U8 *)pBlock->pData + pIter->idx * pIter->pState->typeSize;
}
