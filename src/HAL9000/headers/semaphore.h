#pragma once

#include "mutex.h"


typedef struct _SEMAPHORE
{
	MUTEX SemaphoreMutex;
	 
	_Guarded_by_(SemaphoreMutex)
	DWORD           Value;
	_Guarded_by_(SemaphoreMutex)

	LIST_ENTRY ListHead;
	
} SEMAPHORE, * PSEMAPHORE;

void
SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
);

void
SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);

void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);