#pragma once

typedef enum _SYSCALL_ID
{
    SyscallIdIdentifyVersion,

	//syscall memset which does a memset on a requested virual address
	SyscallIdMemset,
	//syscalls to set and get the global variable 

	SyscallIdSetGlobalVariable,
	SyscallIdGetGlobalVariable,
	//syscall to disable all the other syscalls
	SyscallIdDisableSyscalls,
    // Thread Management
    SyscallIdThreadExit,
    SyscallIdThreadCreate,
    SyscallIdThreadGetTid,
    SyscallIdThreadWaitForTermination,
    SyscallIdThreadCloseHandle,

    // Process Management
    SyscallIdProcessExit,
    SyscallIdProcessCreate,
    SyscallIdProcessGetPid,
    SyscallIdProcessWaitForTermination,
    SyscallIdProcessCloseHandle,

    // Memory management 
    SyscallIdVirtualAlloc,
    SyscallIdVirtualFree,

    // File management
    SyscallIdFileCreate,
    SyscallIdFileClose,
    SyscallIdFileRead,
    SyscallIdFileWrite,

	//labsyscalls
	SyscallIdProcessGetNumberOfPages,
	SyscallIdReadMemory,

	//3. Write a similar system call to write into the memory and try writing into the code area.
	SyscallIdWriteMemmory,

    SyscallIdReserved = SyscallIdFileWrite + 1
} SYSCALL_ID;
