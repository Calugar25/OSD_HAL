#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread_internal.h"
#include "iomu.h"
#include "filesystem.h"

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

		//LOG("THIS IS kk : %x \n ", sysCallId);

        // Dispatch syscalls
        switch (sysCallId)
        {
        case SyscallIdIdentifyVersion:
            status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
            break;
		case SyscallIdProcessGetPid:
			status = SyscallProcessGetPid((UM_HANDLE)pSyscallParameters[0],(PID*)pSyscallParameters[1]);
			break;
        // STUDENT TODO: implement the rest of the syscalls
		case SyscallIdProcessExit:
			status = SyscallProcessExit((STATUS)*pSyscallParameters);
			break;
		case SyscallIdFileWrite:
			status = SyscallFileWrite((UM_HANDLE)pSyscallParameters[0],(PVOID)pSyscallParameters[1]
				, (QWORD)pSyscallParameters[2],(QWORD*) pSyscallParameters[3]
			);
			break;
		case SyscallIdProcessCreate:
			status = SyscallProcessCreate((char*)pSyscallParameters[0], (QWORD)pSyscallParameters[1], (char*)pSyscallParameters[2], (QWORD)pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
			break;
		case SyscallIdThreadExit:
			status = SyscallThreadExit((STATUS)*pSyscallParameters);
			break;
		case SyscallIdProcessWaitForTermination:
			status = SyscallProcessWaitForTermination((UM_HANDLE)pSyscallParameters[0],(STATUS*)pSyscallParameters[1]);
			break;
		case SyscallIdProcessCloseHandle:
			status = SyscallProcessCloseHandle((UM_HANDLE)*pSyscallParameters);
			break;
		case SyscallIdFileCreate:
			status = SyscallFileCreate((char*)pSyscallParameters[0], (QWORD)pSyscallParameters[1], (BOOLEAN)pSyscallParameters[2], (BOOLEAN)pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
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

// STUDENT TODO: implement the rest of the syscalls

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
SyscallFileWrite(
	IN  UM_HANDLE                   FileHandle,
	IN_READS_BYTES(BytesToWrite)
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

	LOG("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);

	*BytesWritten = BytesToWrite;
	return STATUS_SUCCESS;





}



STATUS
SyscallThreadExit(
	IN  STATUS                      ExitStatus
)
{
	ThreadExit(ExitStatus);

	return STATUS_SUCCESS;
}

STATUS
SyscallProcessGetPid(
	IN_OPT  UM_HANDLE               ProcessHandle,
	OUT     PID* ProcessId
)
{


	if (ProcessHandle == UM_INVALID_HANDLE_VALUE)
	{
		
		*ProcessId = GetCurrentProcess()->Id;
		 return STATUS_SUCCESS;
	}
	else
	{
	
		LIST_ITERATOR it;
		ListIteratorInit(&GetCurrentProcess()->pList, &it);
		PLIST_ENTRY pEntry;
		while ((pEntry = ListIteratorNext(&it)) != NULL)
		{
			PPROCESS process = CONTAINING_RECORD(pEntry, PROCESS, pList);
			if (process->processHandle == ProcessHandle)
			{
				*ProcessId = process->Id;
				return STATUS_SUCCESS;
			}
		}
		
		
	}
	return STATUS_UNSUCCESSFUL;

	
}


STATUS
SyscallProcessCreate(
	IN_READS_Z(PathLength)
	char* ProcessPath,
	IN          QWORD               PathLength,
	IN_READS_OPT_Z(ArgLength)
	char* Arguments,
	IN          QWORD               ArgLength,
	OUT         UM_HANDLE* ProcessHandle
) {
	//UNREFERENCED_PARAMETER(PathLength);
	UNREFERENCED_PARAMETER(ArgLength);
	
	if (ProcessPath == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (PathLength == 0)
	{
		return STATUS_UNSUCCESSFUL;
	}


	char fullPath[MAX_PATH];

	snprintf(fullPath, MAX_PATH,
		"%s%s\\%s", IomuGetSystemPartitionPath(), "APPLICATIONS",
		ProcessPath);

	//LOG("this is the full path %s", fullPath);

	PPROCESS process;
	STATUS status;


	status=ProcessCreate(
		fullPath,
		Arguments,
		&process);
	

	if (process != NULL)
	{
		*ProcessHandle = process->processHandle;
	}

	return status;


}


STATUS
SyscallProcessWaitForTermination(
	IN      UM_HANDLE               ProcessHandle,
	OUT     STATUS* TerminationStatus
) {
	STATUS status;
	
	if (UM_INVALID_HANDLE_VALUE != ProcessHandle)
	{	
		
		LIST_ITERATOR it;
		ListIteratorInit(&GetCurrentProcess()->pList, &it);
		PLIST_ENTRY pEntry;
		
		while ((pEntry = ListIteratorNext(&it)) != NULL)
		{
			PPROCESS process = CONTAINING_RECORD(pEntry, PROCESS, pList);
			if (process->processHandle == ProcessHandle)
			{
				ProcessWaitForTermination(process,&status);
				*TerminationStatus = status;
				return STATUS_SUCCESS;
			}
		}
		
	}
	*TerminationStatus = STATUS_UNSUCCESSFUL;
	return STATUS_UNSUCCESSFUL;
	
	

}

STATUS
SyscallProcessCloseHandle(
	IN      UM_HANDLE               ProcessHandle
) {
	if (ProcessHandle == UM_INVALID_HANDLE_VALUE)
	{
		return STATUS_UNSUCCESSFUL;
	}
	else {
		LIST_ITERATOR it;
		ListIteratorInit(&GetCurrentProcess()->pList, &it);
		PLIST_ENTRY pEntry;

		while ((pEntry = ListIteratorNext(&it)) != NULL)
		{
			PPROCESS process = CONTAINING_RECORD(pEntry, PROCESS, pList);
			if (process->processHandle == ProcessHandle)
			{
				RemoveEntryList(&process->pList);
				ProcessCloseHandle(process);
				
				return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_UNSUCCESSFUL;
}
/*STATUS
IoCreateFile(
    OUT_PTR     PFILE_OBJECT*           Handle,
    IN_Z        char*                   FileName,
    IN          BOOLEAN                 Directory,
    IN          BOOLEAN                 Create,
    IN          BOOLEAN                 Asynchronous
    )
{*/

STATUS
SyscallFileCreate(
	IN_READS_Z(PathLength)
	char* Path,
	IN          QWORD                   PathLength,
	IN          BOOLEAN                 Directory,
	IN          BOOLEAN                 Create,
	OUT         UM_HANDLE* FileHandle
) {
	
	UNREFERENCED_PARAMETER(PathLength);
	STATUS status;
	PFILE_OBJECT fileObject;

	STATUS memory = MmuIsBufferValid((PVOID)Path, PathLength, PAGE_RIGHTS_READ, GetCurrentProcess());

	if (Path == NULL||memory!=STATUS_UNSUCCESSFUL||PathLength<1)
	{
		return STATUS_UNSUCCESSFUL;
	}
	else {
	

		char fullPath[MAX_PATH];

		snprintf(fullPath, MAX_PATH,
			"%s\\%s", IomuGetSystemPartitionPath(),
			Path);
		

		if (!Create) {

			
			status = IoCreateFile(
				&fileObject,
				fullPath,
				Directory,
				Create,
				FALSE);
			if (status != STATUS_SUCCESS)
			{
				*FileHandle = 0;
				return STATUS_UNSUCCESSFUL;
			}
			else
			{
				*FileHandle = 22;
			}
		}
		*FileHandle = 0;
		return STATUS_SUCCESS;
	}



	
}
