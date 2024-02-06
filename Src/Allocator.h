#pragma once
#include <stddef.h>
#include <RUVM.h>

void ruvmAllocatorSetCustom(RuvmAllocator *pAlloc, RuvmAllocator *pAllocator);
void ruvmAllocatorSetDefault(RuvmAllocator *pAlloc);
