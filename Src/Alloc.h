#pragma once
#include <stddef.h>
#include <RUVM.h>

void ruvmAllocSetCustom(RuvmAlloc *pAlloc, RuvmAlloc *pCustomAlloc);
void ruvmAllocSetDefault(RuvmAlloc *pAlloc);
