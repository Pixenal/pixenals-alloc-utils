#include <malloc.h>
#include <stdio.h>
#include <assert.h>

#include <Alloc.h>
#include <Context.h>
#include <Error.h>

void ruvmAllocSetCustom(RuvmAlloc *pAlloc, RuvmAlloc *pCustomAlloc) {
	RUVM_ASSERT("", pAlloc && pCustomAlloc);
	if (!pCustomAlloc->pMalloc || !pCustomAlloc->pCalloc || !pCustomAlloc->pFree) {
		printf("Failed to set custom alloc. One or more functions were NULL");
		return;
	}
	*pAlloc = *pCustomAlloc;
}

void ruvmAllocSetDefault(RuvmAlloc *pAlloc) {
	RUVM_ASSERT("", pAlloc);
	pAlloc->pMalloc = malloc;
	pAlloc->pCalloc = calloc;
	pAlloc->pFree = free;
	pAlloc->pRealloc = realloc;
}
