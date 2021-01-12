#include "common_lib.h"
#include "syscall_if.h"
#include "um_lib_helper.h"

STATUS
__main(
    DWORD       argc,
    char**      argv
)
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
	//call to ThreadGetNumberOfSiblings 
	DWORD* NoSib;
	STATUS status = SyscallThreadGetNumberOfSiblings(&NoSib);
    return STATUS_SUCCESS;
}