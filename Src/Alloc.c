#include <malloc.h>

#include <Alloc.h>
#include <Context.h>

void ruvmAllocSetCustom(RuvmAlloc *pAlloc, RuvmAlloc *pCustomAlloc) {
	if (!pCustomAlloc->pMalloc || !pCustomAlloc->pCalloc || !pCustomAlloc->pFree) {
		printf("Failed to set custom alloc. One or more functions were NULL");
		return;
	}
	*pAlloc = *pCustomAlloc;
}

void ruvmAllocSetDefault(RuvmAlloc *pAlloc) {
	pAlloc->pMalloc = malloc;
	pAlloc->pCalloc = calloc;
	pAlloc->pFree = free;
	pAlloc->pRealloc = realloc;
}
