// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "config.h"

#if CONFIG_M17
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"

// Allocation hooks for the separately built embedded Codec 2 archive.
void *codec2_malloc(size_t size)
{
	return pvPortMalloc(size);
}

void *codec2_calloc(size_t count, size_t size)
{
	if (size && count > SIZE_MAX / size)
		return NULL;
	size_t bytes = count * size;
	void *ptr = pvPortMalloc(bytes);
	if (ptr)
		memset(ptr, 0, bytes);
	return ptr;
}

void codec2_free(void *ptr)
{
	if (ptr)
		vPortFree(ptr);
}
#endif
