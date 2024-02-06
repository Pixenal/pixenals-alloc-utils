#include <malloc.h>

#include <Allocator.h>
#include <Context.h>

void ruvmAllocatorSetCustom(RuvmAllocator *pAlloc, RuvmAllocator *pAllocator) {
	if (!pAllocator->pMalloc || !pAllocator->pCalloc || !pAllocator->pFree) {
		printf("Failed to set custom allocator. One or more functions were NULL");
		return;
	}
	*pAlloc = *pAllocator;
}

void ruvmAllocatorSetDefault(RuvmAllocator *pAlloc) {
	pAlloc->pMalloc = malloc;
	pAlloc->pCalloc = calloc;
	pAlloc->pFree = free;
}
