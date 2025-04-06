/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#pragma once
#include <stddef.h>

#include <uv_stucco.h>
#include <types.h>
#include <error.h>

typedef struct LinAllocBlock {
	void *pData;
	I32 size;
	I32 count;
	I32 lessThan;
} LinAllocBlock;

typedef struct LinAlloc {
	LinAllocBlock *pBlockArr;
	StucAlloc alloc;
	I32 blockIdx;
	I32 blockCount;
	I32 blockArrSize;
	I32 typeSize;
	I32 linIdx;
	bool zeroOnClear;
	bool valid;
} LinAlloc;

typedef struct LinAllocIter {
	const LinAlloc *pState;
	Range range;
	I32 rangeSize;
	I32 count;
	I32 block;
	I32 idx;
} LinAllocIter;

void stucAllocSetCustom(StucAlloc *pAlloc, StucAlloc *pCustomAlloc);
void stucAllocSetDefault(StucAlloc *pAlloc);

void stucLinAllocInit(
	const StucAlloc *pAlloc,
	LinAlloc *pHandle,
	I32 size,
	I32 initLen,
	bool zeroOnClear
);
I32 stucLinAlloc(LinAlloc *pHandle, void **ppData, I32 len);
void stucLinAllocClear(LinAlloc *pHandle);
void stucLinAllocDestroy(LinAlloc *pHandle);

void *stucLinAllocIdx(LinAlloc *pHandle, I32 idx);
const void *stucLinAllocIdxConst(const LinAlloc *pState, I32 idx);

inline
I32 stucLinAllocGetCount(const LinAlloc *pHandle) {
	STUC_ASSERT(
		"",
		pHandle->valid && pHandle->pBlockArr != NULL
	);
	I32 total = 0;
	for (I32 i = 0; i <= pHandle->blockIdx; ++i) {
		total += pHandle->pBlockArr[i].count;
	}
	return total;
}

void stucLinAllocIterInit(LinAlloc *pState, Range range, LinAllocIter *pIter);

inline
bool stucLinAllocIterAtEnd(const LinAllocIter *pIter) {
	return
		pIter->count >= pIter->rangeSize ||
		pIter->block > pIter->pState->blockIdx;
}

inline
void stucLinAllocIterInc(LinAllocIter *pIter) {
	STUC_ASSERT(
		"",
		pIter->block <= pIter->pState->blockIdx &&
		pIter->idx < pIter->pState->pBlockArr[pIter->block].count
	);
	const LinAllocBlock *pBlock = pIter->pState->pBlockArr + pIter->block;
	pIter->idx++;
	if (pIter->idx == pBlock->count) {
		pIter->block++;
		pIter->idx = 0;
	}
	pIter->count++;
}

inline
void *stucLinAllocGetItem(const LinAllocIter *pIter) {
	STUC_ASSERT(
		"",
		pIter->block <= pIter->pState->blockIdx &&
		pIter->idx < pIter->pState->pBlockArr[pIter->block].count
	);
	const LinAllocBlock *pBlock = pIter->pState->pBlockArr + pIter->block;
	return (U8 *)pBlock->pData + pIter->idx * pIter->pState->typeSize;
}
