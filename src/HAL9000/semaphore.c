#include "HAL9000.h"
#include "semaphore.h"
#include "thread_internal.h"

void SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value)
{
	INTR_STATE oldState;
	oldState=CpuIntrDisable();



	MutexAcquire(&Semaphore->SemaphoreMutex);
	Semaphore->Value += Value;



	while (!IsListEmpty(&Semaphore->ListHead))
	{
		PLIST_ENTRY listEntry = RemoveHeadList(&Semaphore->ListHead);
		PTHREAD pThread = CONTAINING_RECORD(listEntry, THREAD, ReadyList);
		ThreadUnblock(pThread);

	}
	CpuIntrSetState(oldState);
	MutexRelease(&Semaphore->SemaphoreMutex);


}


void SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
) {

	INTR_STATE oldState;

	oldState = CpuIntrDisable();



	if (Semaphore->Value < Value)
	{
		PTHREAD currenThread = GetCurrentThread();
		InsertTailList(&Semaphore->ListHead,&currenThread->ReadyList);
		MutexRelease(&Semaphore->SemaphoreMutex);

		
		ThreadBlock();
		CpuIntrSetState(oldState);
		MutexAcquire(&Semaphore->SemaphoreMutex);
	}

	Semaphore->Value -= Value;

	MutexRelease(&Semaphore->SemaphoreMutex);



}


void SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
)
{
	ASSERT(Semaphore != NULL);

	Semaphore->Value = InitialValue;
	InitializeListHead(&Semaphore->ListHead);

	MutexInit(&Semaphore->SemaphoreMutex,0);

	memzero(Semaphore, sizeof(SEMAPHORE));
}