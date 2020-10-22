#include "HAL9000.h"
#include "ex_timer.h"
#include "iomu.h"
#include "thread_internal.h"
#include "lock_common.h"
#include "system.c"

static struct _GLOBAL_TIMER_LIST m_globalTimerList;

//PFUNC_CompareFunction ExTimerCompareListElems(PLIST_ENTRY p1, PLIST_ENTRY p2) 

void ExTimerSystemPreinit()
{
	InitializeListHead(&m_globalTimerList.TimerListHead);
	LockInit(&m_globalTimerList.TimerListLock);
}

void ExTimerCompareListElems(PLIST_ENTRY t1, PLIST_ENTRY t2, PVOID context)
{

 }

void ExTimerCheck(
	IN 	PEX_TIMER Timer
) {

	ASSERT(NULL != Timer);



	if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs)
	{
		ExEventSignal(&Timer->TimerEvent);
	}



}

void ExTimerCheckAll()
{
	LockAcquire(&m_globalTimerList, FALSE);

	LIST_ITERATOR it;
	ListIteratorInit(&m_globalTimerList, &it);
	
	 PLIST_ENTRY pEntry;
	 while ((pEntry = ListIteratorNext(&it)) != NULL)
	 {
		 PLIST_ENTRY pFoo = CONTAINING_RECORD(pEntry, LIST_ENTRY, Blink);
	      if (pFoo->SomeData == DataToSearch) RemoveEntryList(pEntry);
	 }

}

STATUS
ExTimerInit(
    OUT     PEX_TIMER       Timer,
    IN      EX_TIMER_TYPE   Type,
    IN      QWORD           Time
    )
{
    STATUS status;

	//ExTimerInit(): add the new timer in the global timer list;
	ASSERT(NULL != Timer);

	//adding a new timer in the list
	LockAcquire(&m_globalTimerList.TimerListLock,FALSE);

	InsertOrderedList(&m_globalTimerList.TimerListHead, &Timer->TimerListElem, );

	//1. when and how to initialize it;
	
	ExEventInit(&Timer->TimerEvent,ExEventTypeNotification,FALSE);


    if (NULL == Timer)
    {
        return STATUS_INVALID_PARAMETER1;
    }

    if (Type > ExTimerTypeMax)
    {
        return STATUS_INVALID_PARAMETER2;
    }

    status = STATUS_SUCCESS;

    memzero(Timer, sizeof(EX_TIMER));

    Timer->Type = Type;
    if (Timer->Type != ExTimerTypeAbsolute)
    {
        // relative time

        // if the time trigger time has already passed the timer will
        // be signaled after the first scheduler tick
        Timer->TriggerTimeUs = IomuGetSystemTimeUs() + Time;
        Timer->ReloadTimeUs = Time;
    }
    else
    {
        // absolute
        Timer->TriggerTimeUs = Time;
    }

    return status;
}



void
ExTimerStart(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = TRUE;
}

void
ExTimerStop(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = FALSE;
	//this immediately lead us to the conclusion that one place to call ExEventSignal()
	//was the function ExTimerStop()

	ExEventSignal(&Timer->TimerEvent);

}

void
ExTimerWait(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }
	//. replacing the busy-waited loop and blocking the waiting thread
	ExEventWaitForSignal(&Timer->TimerEvent);

    while (IomuGetSystemTimeUs() < Timer->TriggerTimeUs && Timer->TimerStarted)
    {
        ThreadYield();
    }
}

void
ExTimerUninit(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);
	//a) acquire the lock that protects the global timer list (calling LockAcquire()

	LockAcquire(&m_globalTimerList.TimerListLock, FALSE);
	RemoveEntryList(&Timer->TimerListElem);
	LockRelease(&m_globalTimerList.TimerListLock,TRUE);
    ExTimerStop(Timer);

    Timer->TimerUninited = TRUE;
}

INT64
ExTimerCompareTimers(
    IN      PEX_TIMER     FirstElem,
    IN      PEX_TIMER     SecondElem
)
{
    return FirstElem->TriggerTimeUs - SecondElem->TriggerTimeUs;
}