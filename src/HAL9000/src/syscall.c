#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread.h"
#include "vmm.h"
#include "thread_internal.h"

extern void SyscallEntry();

#define SYSCALL_IF_VERSION_KM       SYSCALL_IMPLEMENTED_IF_VERSION

void
SyscallHandler(
    INOUT   COMPLETE_PROCESSOR_STATE    *CompleteProcessorState
    )
{
    SYSCALL_ID sysCallId;
    PQWORD pSyscallParameters;
    PQWORD pParameters;
    STATUS status;
    REGISTER_AREA* usermodeProcessorState;

    ASSERT(CompleteProcessorState != NULL);

    // It is NOT ok to setup the FMASK so that interrupts will be enabled when the system call occurs
    // The issue is that we'll have a user-mode stack and we wouldn't want to receive an interrupt on
    // that stack. This is why we only enable interrupts here.
    ASSERT(CpuIntrGetState() == INTR_OFF);
    CpuIntrSetState(INTR_ON);

    LOG_TRACE_USERMODE("The syscall handler has been called!\n");

    status = STATUS_SUCCESS;
    pSyscallParameters = NULL;
    pParameters = NULL;
    usermodeProcessorState = &CompleteProcessorState->RegisterArea;

    __try
    {
        if (LogIsComponentTraced(LogComponentUserMode))
        {
            DumpProcessorState(CompleteProcessorState);
        }

        // Check if indeed the shadow stack is valid (the shadow stack is mandatory)
        pParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp];
        status = MmuIsBufferValid(pParameters, SHADOW_STACK_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            LOG_FUNC_ERROR("MmuIsBufferValid", status);
            __leave;
        }

        sysCallId = usermodeProcessorState->RegisterValues[RegisterR8];

        LOG_TRACE_USERMODE("System call ID is %u\n", sysCallId);

        // The first parameter is the system call ID, we don't care about it => +1
        pSyscallParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp] + 1;

        // Dispatch syscalls
        switch (sysCallId)
        {
        case SyscallIdIdentifyVersion:
            status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
            break;
        // STUDENT TODO: implement the rest of the syscalls
		case SyscallIdFileWrite:
			status = SyscallFileWrite((UM_HANDLE)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (QWORD)pSyscallParameters[2], (QWORD*)pSyscallParameters[3]);
			break;
		case SyscallIdProcessExit:
			SyscallProcessExit((STATUS)*pSyscallParameters);
			break;
		case SyscallIdThreadExit:
			status = SyscallThreadExit((STATUS)*pSyscallParameters);
			break;
		case SyscallIdReadMemory:
			
				status = SyscallReadMemory((PBYTE)pSyscallParameters[0], (PBYTE)pSyscallParameters[1]);
				break;
		case SyscallIdThreadCreate:
			status = SyscallThreadCreate((PFUNC_ThreadStart)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (UM_HANDLE*)pSyscallParameters[2]);
			break;
		case SyscallIdThreadGetTid:
			status = SyscallThreadGetTid((UM_HANDLE)pSyscallParameters[0], (TID*)pSyscallParameters[1]);
			break;
		case SyscallIdProcessGetPid:
			status = SyscallProcessGetPid((UM_HANDLE)pSyscallParameters[0], (PID*)pSyscallParameters[1]);
			break;
		case SyscallIdThreadWaitForTermination:
			status =SyscallThreadWaitForTermination((UM_HANDLE)pSyscallParameters[0], (STATUS*)pSyscallParameters[1]);
			break;
		case SyscallIdThreadCloseHandle:
			status = SyscallThreadCloseHandle((UM_HANDLE)*pSyscallParameters);
			break;
		case SyscallIdVirtualAlloc:
			status = SyscallVirtualAlloc((PVOID)pSyscallParameters[0], (QWORD)pSyscallParameters[1], (VMM_ALLOC_TYPE)pSyscallParameters[2], (PAGE_RIGHTS)pSyscallParameters[3], (UM_HANDLE)pSyscallParameters[4], (QWORD)pSyscallParameters[5],(PVOID)pSyscallParameters[6]);
			break;
        default:
            LOG_ERROR("Unimplemented syscall called from User-space!\n");
            status = STATUS_UNSUPPORTED;
            break;
        }

    }
    __finally
    {
        LOG_TRACE_USERMODE("Will set UM RAX to 0x%x\n", status);

        usermodeProcessorState->RegisterValues[RegisterRax] = status;

        CpuIntrSetState(INTR_OFF);
    }
}

void
SyscallPreinitSystem(
    void
    )
{

}

STATUS
SyscallInitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

STATUS
SyscallUninitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

void
SyscallCpuInit(
    void
    )
{
    IA32_STAR_MSR_DATA starMsr;
    WORD kmCsSelector;
    WORD umCsSelector;

    memzero(&starMsr, sizeof(IA32_STAR_MSR_DATA));

    kmCsSelector = GdtMuGetCS64Supervisor();
    ASSERT(kmCsSelector + 0x8 == GdtMuGetDS64Supervisor());

    umCsSelector = GdtMuGetCS32Usermode();
    /// DS64 is the same as DS32
    ASSERT(umCsSelector + 0x8 == GdtMuGetDS32Usermode());
    ASSERT(umCsSelector + 0x10 == GdtMuGetCS64Usermode());

    // Syscall RIP <- IA32_LSTAR
    __writemsr(IA32_LSTAR, (QWORD) SyscallEntry);

    LOG_TRACE_USERMODE("Successfully set LSTAR to 0x%X\n", (QWORD) SyscallEntry);

    // Syscall RFLAGS <- RFLAGS & ~(IA32_FMASK)
    __writemsr(IA32_FMASK, RFLAGS_INTERRUPT_FLAG_BIT);

    LOG_TRACE_USERMODE("Successfully set FMASK to 0x%X\n", RFLAGS_INTERRUPT_FLAG_BIT);

    // Syscall CS.Sel <- IA32_STAR[47:32] & 0xFFFC
    // Syscall DS.Sel <- (IA32_STAR[47:32] + 0x8) & 0xFFFC
    starMsr.SyscallCsDs = kmCsSelector;

    // Sysret CS.Sel <- (IA32_STAR[63:48] + 0x10) & 0xFFFC
    // Sysret DS.Sel <- (IA32_STAR[63:48] + 0x8) & 0xFFFC
    starMsr.SysretCsDs = umCsSelector;

    __writemsr(IA32_STAR, starMsr.Raw);

    LOG_TRACE_USERMODE("Successfully set STAR to 0x%X\n", starMsr.Raw);
}

// SyscallIdIdentifyVersion
STATUS
SyscallValidateInterface(
    IN  SYSCALL_IF_VERSION          InterfaceVersion
)
{
    LOG_TRACE_USERMODE("Will check interface version 0x%x from UM against 0x%x from KM\n",
        InterfaceVersion, SYSCALL_IF_VERSION_KM);

    if (InterfaceVersion != SYSCALL_IF_VERSION_KM)
    {
        LOG_ERROR("Usermode interface 0x%x incompatible with KM!\n", InterfaceVersion);
        return STATUS_INCOMPATIBLE_INTERFACE;
    }

    return STATUS_SUCCESS;
}


STATUS
SyscallFileWrite(
	IN  UM_HANDLE                   FileHandle,
	OUT_WRITES_BYTES(BytesToRead)
	PVOID                       Buffer,
	IN  QWORD                       BytesToWrite,
	OUT QWORD* BytesWritten
)
{

	*BytesWritten = 0;

	if (FileHandle != UM_FILE_HANDLE_STDOUT)
	{
		return STATUS_UNSUCCESSFUL;
	}

	STATUS status = MmuIsBufferValid(Buffer, BytesToWrite, PAGE_RIGHTS_READ, GetCurrentProcess());

	if (!SUCCEEDED(status))
	{
		return status;
	}

	MmuIsBufferValid(BytesWritten, sizeof(QWORD), PAGE_RIGHTS_WRITE, GetCurrentProcess());

	LOG_TRACE_USERMODE("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);
	*BytesWritten = BytesToWrite;

	 return STATUS_SUCCESS;
}


STATUS
SyscallProcessExit(
	IN      STATUS                  ExitStatus
)
{
	PPROCESS process = GetCurrentProcess();
	process->TerminationStatus = ExitStatus;
	ProcessTerminate(process);
	return STATUS_SUCCESS;
}

STATUS
 SyscallThreadExit(
	IN      STATUS                  ExitStatus
)
{
	ThreadExit(ExitStatus);

	return STATUS_SUCCESS;
}


// STUDENT TODO: implement the rest of the syscalls

STATUS
 SyscallProcessGetNumberOfPages(
	OUT     DWORD * PagesCommitted,
	OUT     DWORD * PagesReserved
	 )
{
	UNREFERENCED_PARAMETER(PagesCommitted);
	UNREFERENCED_PARAMETER(PagesReserved);
	
	//PPROCESS currentProcess = GetCurrentProcess();
	
		
	return STATUS_SUCCESS;
}

STATUS
SyscallReadMemory(
	IN_READS(1)     PBYTE   Address,
	OUT             PBYTE   ValueRead
) {
	//PHYSICAL_ADDRESS pAddress = MmuGetPhysicalAddress((PVOID)Address);
	UNREFERENCED_PARAMETER(Address);
	UNREFERENCED_PARAMETER(ValueRead);
	//if (!_VmIsKernelAddress(Address))
	//{
		//Now we read the value at this pAddress 
		//ValueRead=GetValueAtAddress(pAddress)
		//Didn't know how exactly i could get that 

		

	//}
	return STATUS_SUCCESS;
}


STATUS
SyscallThreadCreate(
	IN      PFUNC_ThreadStart       StartFunction,
	IN_OPT  PVOID                   Context,
	OUT     UM_HANDLE* ThreadHandle
)
{
	
	
	PTHREAD pThread;
	STATUS status = ThreadCreateEx(
		"my Thread", ThreadPriorityDefault,
		StartFunction,
		Context,
		&pThread,
		GetCurrentProcess()
	);

	

	ThreadHandle = (UM_HANDLE*)pThread;
	return status;
}

STATUS
SyscallThreadGetTid(
	IN_OPT  UM_HANDLE               ThreadHandle,
	OUT     TID* ThreadId
)
{
	LOG("OPP");
	if (ThreadHandle == UM_INVALID_HANDLE_VALUE)
	{
		*ThreadId = GetCurrentThread()->Id;
		return STATUS_SUCCESS;
	}
	else {
		ThreadId = NULL;
		return STATUS_UNSUCCESSFUL;
	}
}

STATUS
SyscallProcessGetPid(
	IN_OPT  UM_HANDLE               ProcessHandle,
	OUT     PID* ProcessId
)
{

	LOG("TTY");
	if (UM_INVALID_HANDLE_VALUE == ProcessHandle)
	{
		PPROCESS process = GetCurrentProcess();
		*ProcessId = process->Id;
		return STATUS_SUCCESS;
	}
	else
	{
		ProcessId = NULL;
		return STATUS_UNSUCCESSFUL;
	}
	
}
	

STATUS
SyscallThreadWaitForTermination(
	IN      UM_HANDLE               ThreadHandle,
	OUT     STATUS* TerminationStatus
)
{
	STATUS status;
	if (ThreadHandle != UM_INVALID_HANDLE_VALUE)
	{

		ThreadWaitForTermination((PTHREAD)ThreadHandle, &status);
		*TerminationStatus = status;

	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
		*TerminationStatus = status;
	}
	return status;
}

STATUS
SyscallThreadCloseHandle(
	IN      UM_HANDLE               ThreadHandle
)
{	

	if (ThreadHandle != UM_INVALID_HANDLE_VALUE)
	{
		ThreadCloseHandle((PTHREAD)ThreadHandle);
		return STATUS_SUCCESS;

	}
	else {
		return STATUS_UNSUCCESSFUL;
	}
}

STATUS
SyscallVirtualAlloc(
	IN_OPT      PVOID                   BaseAddress,
	IN          QWORD                   Size,
	IN          VMM_ALLOC_TYPE          AllocType,
	IN          PAGE_RIGHTS             PageRights,
	IN_OPT      UM_HANDLE               FileHandle,
	IN_OPT      QWORD                   Key,
	OUT         PVOID* AllocatedAddress
) {
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(FileHandle);
	*AllocatedAddress = VmmAllocRegionEx(BaseAddress, Size, AllocType, PageRights, FALSE, NULL, GetCurrentProcess()->VaSpace, GetCurrentProcess()->PagingData, NULL);

	return STATUS_SUCCESS;
}


