/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <alloc.h>

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

void pixalcLinAllocInit(
	const PixalcFPtrs *pAlloc,
	PixalcLinAlloc *pState,
	I32 size,
	I32 initLen,
	bool zeroOnClear
) {
	PIX_ERR_ASSERT("", pAlloc && pState);
	PIX_ERR_ASSERT("", size > 0 && initLen > 0);
	*pState = (PixalcLinAlloc){
		.alloc = *pAlloc,
		.blockArrSize = 1,
		.blockCount = 1,
		.zeroOnClear = zeroOnClear,
		.typeSize = size,
		.valid = true
	};
	pState->pBlockArr = pAlloc->fpMalloc(pState->blockArrSize * sizeof(PixalcLinAllocBlock));
	pState->pBlockArr[0] = (PixalcLinAllocBlock) {
		.size = initLen,
		.pData = pAlloc->fpCalloc(initLen, size)
	};
}

static
void incrementBlock(PixalcLinAlloc *pState, I32 requiredLen) {
	PIX_ERR_ASSERT(
		"",
		pState->blockIdx >= 0 &&
		pState->blockIdx < pState->blockArrSize &&
		pState->blockIdx < pState->blockCount &&
		pState->blockCount <= pState->blockArrSize
	);
	pState->pBlockArr[pState->blockIdx].lessThan = pState->linIdx;
	pState->blockIdx++;
	if (pState->blockIdx == pState->blockArrSize) {
		pState->blockArrSize *= 2;
		pState->pBlockArr = pState->alloc.fpRealloc(
			pState->pBlockArr,
			pState->blockArrSize * sizeof(PixalcLinAllocBlock)
		);
	}
	else if (pState->blockIdx != pState->blockCount) {
		return; //this block was already alloc'ed (blocks were cleared)
	}
	PixalcLinAllocBlock *pNewBlock = pState->pBlockArr + pState->blockIdx;
	I32 oldSize = pState->pBlockArr[pState->blockIdx - 1].size;
	*pNewBlock = (PixalcLinAllocBlock) {.size = (oldSize + requiredLen) * 2};
	pNewBlock->pData = pState->alloc.fpCalloc(pNewBlock->size, pState->typeSize);
	pState->blockCount++;
}

I32 pixalcLinAlloc(PixalcLinAlloc *pState, void **ppData, I32 len) {
	PIX_ERR_ASSERT("", pState && ppData);
	PixalcLinAllocBlock *pBlock = pState->pBlockArr + pState->blockIdx;
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

void pixalcLinAllocClear(PixalcLinAlloc *pState) {
	PIX_ERR_ASSERT("", pState);
	PIX_ERR_ASSERT("", pState->pBlockArr);
	if (!pState->blockIdx && !pState->pBlockArr[0].count) {
		return;
	}
	for (I32 i = pState->blockIdx; i >= 0; --i) {
		PixalcLinAllocBlock *pBlock = pState->pBlockArr + i;
		if (pState->zeroOnClear) {
			memset(pBlock->pData, 0, pBlock->count * pState->typeSize);
		}
		pBlock->count = 0;
		pBlock->lessThan = 0;
	}
	pState->blockIdx = 0;
	pState->linIdx = 0;
}

void pixalcLinAllocDestroy(PixalcLinAlloc *pState) {
	PIX_ERR_ASSERT("", pState);
	PIX_ERR_ASSERT("", pState->pBlockArr);
	PIX_ERR_ASSERT(
		"",
		pState->blockCount >= 0 && pState->blockCount <= pState->blockArrSize
	);
	for (I32 i = 0; i < pState->blockCount; ++i) {
		pState->alloc.fpFree(pState->pBlockArr[i].pData);
	}
	pState->alloc.fpFree(pState->pBlockArr);
	*pState = (PixalcLinAlloc) {0};
}

//binary search through blocks
static
I32 getBlockFromIdx(const PixalcLinAlloc *pState, I32 idx) {
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
	PIX_ERR_ASSERT("", low >= 0 && low <= pState->blockIdx);
	return low;
}

static
I32 getIdxInBlock(const PixalcLinAlloc *pState, I32 block, I32 idx) {
	if (block) {
		return idx - pState->pBlockArr[block - 1].lessThan;
	}
	else {
		return idx;
	}
}

void *pixalcLinAllocIdx(PixalcLinAlloc *pState, I32 idx) {
	PIX_ERR_ASSERT("", pState && pState->valid);
	PIX_ERR_ASSERT("out of range", idx >= 0 && idx < pState->linIdx);
	I32 block = getBlockFromIdx(pState, idx);
	I32 idxInBlock = getIdxInBlock(pState, block, idx);
	PIX_ERR_ASSERT("", idxInBlock < pState->pBlockArr[block].count);
	return (U8 *)pState->pBlockArr[block].pData + idxInBlock * pState->typeSize;
}

const void *pixalcLinAllocIdxConst(const PixalcLinAlloc *pState, I32 idx) {
	return pixalcLinAllocIdx((PixalcLinAlloc *)pState, idx);
}

void pixalcLinAllocIterInit(PixalcLinAlloc *pState, PixtyRange range, PixalcLinAllocIter *pIter) {
	PIX_ERR_ASSERT("", pState);
	I32 block = getBlockFromIdx(pState, range.start);
	I32 idxInBlock = getIdxInBlock(pState, block, range.start);
	PIX_ERR_ASSERT("", range.start >= 0 && range.start < range.end);
	*pIter = (PixalcLinAllocIter){
		.pState = pState,
		.range = range,
		.rangeSize = range.end - range.start,
		.block = block,
		.idx = idxInBlock,
		.count = 0
	};
}
