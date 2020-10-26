#include "HAL9000.h"
#include "ex_timer.h"
#include "iomu.h"
#include "thread_internal.h"
#include "lock_common.h"


static  GLOBAL_TIMER_LIST m_globalTimerList;

//PFUNC_CompareFunction ExTimerCompareListElems(PLIST_ENTRY p1, PLIST_ENTRY p2) 

void ExTimerSystemPreinit()
{

	InitializeListHead(&m_globalTimerList.TimerListHead);
	LockInit(&m_globalTimerList.TimerListLock);
}


INT64(__cdecl ExTimerCompareListElement) (PLIST_ENTRY entry1, PLIST_ENTRY entry2, PVOID arg) {

	UNREFERENCED_PARAMETER(arg);
	PEX_TIMER pTimer1 = CONTAINING_RECORD(entry1, EX_TIMER, TimerListElem);
	PEX_TIMER pTimer2 = CONTAINING_RECORD(entry2, EX_TIMER, TimerListElem);
	return ExTimerCompareTimers(pTimer1, pTimer2);
}



void
Example(
	INOUT   PEX_TIMER       Timer
)
{
	ASSERT(Timer != NULL);

	//a) acquire the lock that protects the global timer list (calling LockAcquire()

	//RemoveEntryList(&Timer->TimerListElem);

	ExTimerStop(Timer);

	Timer->TimerUninited = TRUE;
}


STATUS CheckforEachOne(PLIST_ENTRY ListEntry, PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);
	PEX_TIMER thisTimer = CONTAINING_RECORD(ListEntry, EX_TIMER, TimerListElem);

	QWORD time = IomuGetSystemTimeUs();

	if (thisTimer->TriggerTimeUs <= time)
	{

		if (thisTimer->Type == ExTimerTypeRelativePeriodic)
		{
			ExEventSignal(&thisTimer->TimerEvent);
			ExEventClearSignal(&thisTimer->TimerEvent);
			thisTimer->TriggerTimeUs = time + thisTimer->ReloadTimeUs;
			RemoveEntryList(ListEntry);

			InsertOrderedList(&m_globalTimerList.TimerListHead, &thisTimer->TimerListElem, ExTimerCompareListElement, NULL);

		}
		else {
		//INTR_STATE state=*((INTR_STATE*) Context);
		Example(thisTimer);
		RemoveEntryList(ListEntry);
		//LockRelease(&m_globalTimerList.TimerListLock, state);
		
		//LockAcquire(&m_globalTimerList.TimerListLock,&state);

		}

		return STATUS_SUCCESS;
	}
	return STATUS_UNSUPPORTED;
}


/*void ExTimerCheck(
	IN 	PEX_TIMER Timer
) {

	ASSERT(NULL != Timer);



	if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs)
	{
		ExEventSignal(&Timer->TimerEvent);
	}

}*/

void ExTimerCheckAll()
{
	INTR_STATE state;
	//INTR_STATE oldState;

	//oldState = CpuIntrDisable();
	LockAcquire(&m_globalTimerList.TimerListLock, &state);

	ForEachElementExecute(&m_globalTimerList.TimerListHead,CheckforEachOne, 0, TRUE);
	LockRelease(&m_globalTimerList.TimerListLock, state);
	//CpuIntrSetState(oldState);


}

STATUS
ExTimerInit(
    OUT     PEX_TIMER       Timer,
    IN      EX_TIMER_TYPE   Type,
    IN      QWORD           Time
    )
{
    STATUS status;

	INTR_STATE state;

	//ExTimerInit(): add the new timer in the global timer list;
	ASSERT(NULL != Timer);

	//adding a new timer in the list
	LockAcquire(&m_globalTimerList.TimerListLock,&state);

	InsertOrderedList(&m_globalTimerList.TimerListHead, &Timer->TimerListElem, ExTimerCompareListElement,NULL );

	LockRelease(&m_globalTimerList.TimerListLock, state);
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
	INTR_STATE state;
	

	LockAcquire(&m_globalTimerList.TimerListLock, &state);
	RemoveEntryList(&Timer->TimerListElem);
	LockRelease(&m_globalTimerList.TimerListLock, state);
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