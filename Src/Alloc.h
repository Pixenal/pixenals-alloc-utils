#pragma once
#include <stddef.h>
#include <RUVM.h>

void uvsAllocSetCustom(RuvmAlloc *pAlloc, RuvmAlloc *pCustomAlloc);
void uvsAllocSetDefault(RuvmAlloc *pAlloc);
