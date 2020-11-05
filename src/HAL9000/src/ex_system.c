#include "HAL9000.h"
#include "ex_system.h"
#include "thread_internal.h"
#include "ex_timer.h"
#include "iomu.h"




void
ExSystemTimerTick(
    void
    )
{
	ThreadTick();
	ExTimerCheckAll();
	//ExTimerCheck();
}