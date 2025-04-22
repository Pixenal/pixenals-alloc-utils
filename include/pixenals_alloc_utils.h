/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stddef.h>

#include "../../pixenals-types/include/pixenals_types.h"
#include "../../pixenals-error-utils/include/pixenals_error_utils.h"

typedef struct PixalcFPtrs {
	void *(*fpMalloc)(size_t);
	void *(*fpCalloc)(size_t, size_t);
	void (*fpFree)(void *);
	void *(*fpRealloc)(void *, size_t);
} PixalcFPtrs;

typedef struct PixalcLinAllocBlock {
	void *pData;
	int32_t size;
	int32_t count;
	int32_t lessThan;
} PixalcLinAllocBlock;

typedef struct PixalcLinAlloc {
	PixalcLinAllocBlock *pBlockArr;
	PixalcFPtrs alloc;
	int32_t blockIdx;
	int32_t blockCount;
	int32_t blockArrSize;
	int32_t typeSize;
	int32_t linIdx;
	bool zeroOnClear;
	bool valid;
} PixalcLinAlloc;

typedef struct PixalcLinAllocArr {
	PixalcLinAlloc *pArr;
	int32_t size;
	int32_t count;
} PixalcLinAllocArr;

typedef struct PixalcLinAllocIter {
	const PixalcLinAlloc *pState;
	PixtyRange range;
	int32_t rangeSize;
	int32_t count;
	int32_t block;
	int32_t idx;
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

#define PIXALC_DYN_ARR_ADD(t, pAlloc, pDynArr, newIdx)\
	PIX_ERR_ASSERT("", (pDynArr)->count <= (pDynArr)->size);\
	if (!(pDynArr)->size) {\
		PIX_ERR_ASSERT("", !(pDynArr)->pArr);\
		(pDynArr)->size = 4;\
		(pDynArr)->pArr = (pAlloc)->fpMalloc((pDynArr)->size * sizeof(t));\
	}\
	else if ((pDynArr)->count == (pDynArr)->size) {\
		(pDynArr)->size *= 2;\
		(pDynArr)->pArr =\
			(pAlloc)->fpRealloc((pDynArr)->pArr, (pDynArr)->size * sizeof(t));\
	}\
	newIdx = (pDynArr)->count;\
	(pDynArr)->count++;

void pixalcLinAllocInit(
	const PixalcFPtrs *pAlloc,
	PixalcLinAlloc *pHandle,
	int32_t size,
	int32_t initLen,
	bool zeroOnClear
);
//if len > 1, the returned array will be contiguous
int32_t pixalcLinAlloc(PixalcLinAlloc *pHandle, void **ppData, int32_t len);
void pixalcLinAllocClear(PixalcLinAlloc *pHandle);
void pixalcLinAllocDestroy(PixalcLinAlloc *pHandle);

void *pixalcLinAllocIdx(PixalcLinAlloc *pHandle, int32_t idx);
const void *pixalcLinAllocIdxConst(const PixalcLinAlloc *pState, int32_t idx);

static inline
int32_t pixalcLinAllocGetCount(const PixalcLinAlloc *pHandle) {
	PIX_ERR_ASSERT(
		"",
		pHandle->valid && pHandle->pBlockArr != NULL
	);
	int32_t total = 0;
	for (int32_t i = 0; i <= pHandle->blockIdx; ++i) {
		total += pHandle->pBlockArr[i].count;
	}
	return total;
}

void pixalcLinAllocIterInit(PixalcLinAlloc *pState, PixtyRange range, PixalcLinAllocIter *pIter);

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
	return (uint8_t *)pBlock->pData + pIter->idx * pIter->pState->typeSize;
}
