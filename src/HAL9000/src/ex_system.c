#include "HAL9000.h"
#include "ex_system.h"
#include "thread_internal.h"
#include "ex_timer.h"
#include "iomu.h"


void ExTimerCheck(
	IN 	PEX_TIMER Timer
) {

	ASSERT(NULL != Timer);



	if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs)
	{
		ExEventSignal(&Timer->TimerEvent);
	}



}

void
ExSystemTimerTick(
    void
    )
{
	ThreadTick();
	ExTimerCheckAll();
	
}