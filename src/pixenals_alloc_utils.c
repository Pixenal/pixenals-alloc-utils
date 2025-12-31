/* 
SPDX-FileCopyrightText: 2025 Caleb Dawson
SPDX-License-Identifier: Apache-2.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <pixenals_alloc_utils.h>

typedef int8_t I8;
typedef int16_t I16;
typedef int64_t I64;

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

static
I32 checkForFreed(PixalcLinAlloc *pState, void **ppData, I32 len) {
	if (pState->freed.count) {
		PIX_ERR_ASSERT("", pState->freed.pArr);
		//TODO replace freed arr with a binary tree,
		//this'll allow not just faster search, but the ability to check
		//for overlap in RegionClear
		for (I32 i = 0; i < pState->freed.count; ++i) {
			PixalcRegion *pRegion = pState->freed.pArr + i;
			if (len <= pRegion->len) {
				I32 blockIdx = getBlockFromIdx(pState, pRegion->idx);
				PixalcLinAllocBlock *pBlock = pState->pBlockArr + blockIdx;
				I32 idxInBlock = getIdxInBlock(pState, blockIdx, pRegion->idx);
				*ppData = (U8 *)pBlock->pData + idxInBlock * pState->typeSize;
				I32 retIdx = pRegion->idx;
				if (i != pState->freed.count - 1) {
					memmove(
						pRegion,
						pRegion + 1,
						sizeof(PixalcRegion) * (pState->freed.count - i - 1)
					);
				}
				--pState->freed.count;
				return retIdx;
			}
		}
	}
	return -1;
}

I32 pixalcLinAlloc(PixalcLinAlloc *pState, void **ppData, I32 len) {
	PIX_ERR_ASSERT("", pState && ppData);
	*ppData = NULL;
	I32 retIdx = checkForFreed(pState, ppData, len);
	PIX_ERR_ASSERT("", !(retIdx != -1 ^ !!*ppData))
	if (*ppData) {
		return retIdx;
	}
	PixalcLinAllocBlock *pBlock = pState->pBlockArr + pState->blockIdx;
	if (pBlock->count == pBlock->size || pBlock->count + len > pBlock->size) {
		incrementBlock(pState, len);
		pBlock = pState->pBlockArr + pState->blockIdx;
	}
	*ppData = (U8 *)pBlock->pData + pBlock->count * pState->typeSize;
	retIdx = pState->linIdx;
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
	if (pState->freed.count) {
		pState->freed.count = 0;
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
	if (pState->freed.pArr) {
		pState->alloc.fpFree(pState->freed.pArr);
	}
	pState->alloc.fpFree(pState->pBlockArr);
	*pState = (PixalcLinAlloc) {0};
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

void pixalcLinAllocRegionClear(PixalcLinAlloc *pState, void *pStart, I32 len) {
	PIX_ERR_ASSERT("", pState && pStart && len > 0);
	I32 linIdx = 0;
	for (I32 i = 0; i < pState->blockCount; ++i) {
		const PixalcLinAllocBlock *pBlock = pState->pBlockArr + i;
		if (pStart < pBlock->pData) {
			linIdx += pBlock->count;
			continue;
		}
		intptr_t offset = (intptr_t)pStart - (intptr_t)pBlock->pData;
		offset /= pState->typeSize;
		if (offset >= pBlock->count) {
			continue;
		}
		PIX_ERR_ASSERT(
			"specificed address isn't aligned with type",
			(U8 *)pBlock->pData + offset * pState->typeSize == pStart
		);
		PIX_ERR_ASSERT(
			"specified region length is invalid",
			offset + len <= pBlock->count
		);
		linIdx += offset;
		I32 freedIdx = 0;
		PIXALC_DYN_ARR_ADD(PixalcRegion, &pState->alloc, &pState->freed, freedIdx);
		pState->freed.pArr[freedIdx].idx = linIdx;
		pState->freed.pArr[freedIdx].len = len;
		memset(pStart, 0, len * pState->typeSize);
		return;
	}
	PIX_ERR_ASSERT("specified address is invalid", false);
}
