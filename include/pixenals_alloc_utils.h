/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stddef.h>

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

void pixalcLinAllocInit(
	const PixalcFPtrs *pAlloc,
	PixalcLinAlloc *pHandle,
	I32 size,
	I32 initLen,
	bool zeroOnClear
);
//if len > 1, the returned array will be contiguous
I32 pixalcLinAlloc(PixalcLinAlloc *pHandle, void **ppData, I32 len);
void pixalcLinAllocClear(PixalcLinAlloc *pHandle);
void pixalcLinAllocDestroy(PixalcLinAlloc *pHandle);

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
