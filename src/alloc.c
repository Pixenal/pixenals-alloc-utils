/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <alloc.h>
#include <context.h>

void stucAllocSetCustom(StucAlloc *pAlloc, StucAlloc *pCustomAlloc) {
	STUC_ASSERT("", pAlloc && pCustomAlloc);
	if (!pCustomAlloc->fpMalloc || !pCustomAlloc->fpCalloc || !pCustomAlloc->fpFree) {
		printf("Failed to set custom alloc. One or more functions were NULL");
		return;
	}
	*pAlloc = *pCustomAlloc;
}

void stucAllocSetDefault(StucAlloc *pAlloc) {
	STUC_ASSERT("", pAlloc);
	pAlloc->fpMalloc = malloc;
	pAlloc->fpCalloc = calloc;
	pAlloc->fpFree = free;
	pAlloc->fpRealloc = realloc;
}

void stucLinAllocInit(
	const StucAlloc *pAlloc,
	LinAlloc *pState,
	I32 size,
	I32 initLen,
	bool zeroOnClear
) {
	STUC_ASSERT("", pAlloc && pState);
	STUC_ASSERT("", size > 0 && initLen > 0);
	*pState = (LinAlloc){
		.alloc = *pAlloc,
		.blockArrSize = 1,
		.blockCount = 1,
		.zeroOnClear = zeroOnClear,
		.typeSize = size,
		.valid = true
	};
	pState->pBlockArr = pAlloc->fpMalloc(pState->blockArrSize * sizeof(LinAllocBlock));
	pState->pBlockArr[0] = (LinAllocBlock) {
		.size = initLen,
		.pData = pAlloc->fpCalloc(initLen, size)
	};
}

static
void incrementBlock(LinAlloc *pState, I32 requiredLen) {
	STUC_ASSERT(
		"",
		pState->blockIdx >= 0 &&
		pState->blockIdx < pState->blockArrSize &&
		pState->blockIdx < pState->blockCount &&
		pState->blockCount <= pState->blockArrSize
	);
	LinAllocBlock *pOldBlock = pState->pBlockArr + pState->blockIdx;
	pOldBlock->lessThan = pState->linIdx;
	pState->blockIdx++;
	if (pState->blockIdx == pState->blockArrSize) {
		pState->blockArrSize *= 2;
		pState->pBlockArr = pState->alloc.fpRealloc(
			pState->pBlockArr,
			pState->blockArrSize * sizeof(LinAllocBlock)
		);
	}
	else if (pState->blockIdx != pState->blockCount) {
		return; //this block was already alloc'ed (blocks were cleared)
	}
	LinAllocBlock *pNewBlock = pState->pBlockArr + pState->blockIdx;
	*pNewBlock = (LinAllocBlock) {.size = (pOldBlock->size + requiredLen) * 2};
	pNewBlock->pData = pState->alloc.fpCalloc(pNewBlock->size, pState->typeSize);
	pState->blockCount++;
}

I32 stucLinAlloc(LinAlloc *pState, void **ppData, I32 len) {
	STUC_ASSERT("", pState && ppData);
	LinAllocBlock *pBlock = pState->pBlockArr + pState->blockIdx;
	if (pBlock->count == pBlock->size || pBlock->count + len > pBlock->size) {
		incrementBlock(pState, len);
		pBlock = pState->pBlockArr + pState->blockIdx;
	}
	*ppData = (U8 *)pBlock->pData + pBlock->count * pState->typeSize;
	I32 retIdx = pState->linIdx;
	pBlock->count += len;
	pState->linIdx += len;
	return retIdx;
}

void stucLinAllocClear(LinAlloc *pState) {
	STUC_ASSERT("", pState);
	STUC_ASSERT("", pState->pBlockArr);
	if (!pState->blockIdx && !pState->pBlockArr[0].count) {
		return;
	}
	for (I32 i = pState->blockIdx; i >= 0; --i) {
		LinAllocBlock *pBlock = pState->pBlockArr + i;
		if (pState->zeroOnClear) {
			memset(pBlock->pData, 0, pBlock->count * pState->typeSize);
		}
		pBlock->count = 0;
		pBlock->lessThan = 0;
	}
	pState->blockIdx = 0;
	pState->linIdx = 0;
}

void stucLinAllocDestroy(LinAlloc *pState) {
	STUC_ASSERT("", pState);
	STUC_ASSERT("", pState->pBlockArr);
	STUC_ASSERT(
		"",
		pState->blockCount >= 0 && pState->blockCount <= pState->blockArrSize
	);
	for (I32 i = 0; i < pState->blockCount; ++i) {
		pState->alloc.fpFree(pState->pBlockArr[i].pData);
	}
	pState->alloc.fpFree(pState->pBlockArr);
	*pState = (LinAlloc) {0};
}

//binary search through blocks
static
I32 getBlockFromIdx(const LinAlloc *pState, I32 idx) {
	I32 low = 0;
	I32 high = pState->blockIdx;
	I32 mid = 0;
	while (high != low) {
		mid = low + (high - low) / 2;
		if (idx < pState->pBlockArr[mid].lessThan) {
			high = mid;
		}
		else {
			low = mid + 1;
		}
	}
	STUC_ASSERT("", low >= 0 && low <= pState->blockIdx);
	return low;
}

static
I32 getIdxInBlock(const LinAlloc *pState, I32 block, I32 idx) {
	if (block) {
		return idx - pState->pBlockArr[block - 1].lessThan;
	}
	else {
		return idx;
	}
}

void *stucLinAllocIdx(LinAlloc *pState, I32 idx) {
	STUC_ASSERT("", pState && pState->valid);
	STUC_ASSERT("out of range", idx >= 0 && idx < pState->linIdx);
	I32 block = getBlockFromIdx(pState, idx);
	I32 idxInBlock = getIdxInBlock(pState, block, idx);
	STUC_ASSERT("", idxInBlock < pState->pBlockArr[block].count);
	return (U8 *)pState->pBlockArr[block].pData + idxInBlock * pState->typeSize;
}

const void *stucLinAllocIdxConst(const LinAlloc *pState, I32 idx) {
	return stucLinAllocIdx((LinAlloc *)pState, idx);
}

void stucLinAllocIterInit(LinAlloc *pState, Range range, LinAllocIter *pIter) {
	STUC_ASSERT("", pState);
	I32 block = getBlockFromIdx(pState, range.start);
	I32 idxInBlock = getIdxInBlock(pState, block, range.start);
	STUC_ASSERT("", range.start >= 0 && range.start < range.end);
	*pIter = (LinAllocIter){
		.pState = pState,
		.range = range,
		.rangeSize = range.end - range.start,
		.block = block,
		.idx = idxInBlock,
		.count = 0
	};
}
