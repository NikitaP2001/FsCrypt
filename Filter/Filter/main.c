#include "filter.h"
#include "FltDbg.h"
#include "main.h"

PFLT_FILTER gFilterHandle;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FilterUnload)
#endif

// Supported operations
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_READ,
      0,
      SwapPreReadBuffers,
      SwapPostReadBuffers },

    { IRP_MJ_WRITE,
      0,
      SwapPreWriteBuffers,
      SwapPostWriteBuffers },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      SwapPreDirCtrlBuffers,
      SwapPostDirCtrlBuffers },

    { IRP_MJ_OPERATION_END }
};

//  Context definitions we currently care about.  Note that the system will
//  create a lookAside list for the volume context because an explicit size
//  of the context is specified.
CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

     { FLT_VOLUME_CONTEXT,
       0,
       CleanupVolumeContext,
       sizeof(VOLUME_CONTEXT),
       CONTEXT_TAG },

     { FLT_CONTEXT_END }
};


// This defines what we want to filter with FltMgr
CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    ContextNotifications,               //  Context
    Callbacks,                          //  Operation callbacks

    FilterUnload,                       //  MiniFilterUnload

    InstanceSetup,                      //  InstanceSetup
    InstanceQueryTeardown,              //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};

NTSTATUS
FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Flags);

    //  Unregister from FLT mgr    
    FltUnregisterFilter(gFilterHandle);

    //  Delete lookaside list    
    ExDeleteNPagedLookasideList(&Pre2PostContextList);

    return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)

{
    NTSTATUS status;
       
    /* Default to NonPagedPoolNx for non paged pool allocations where supported. */
    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
   
    /* Get debug trace flags */
    ReadDriverParameters(RegistryPath);
    
    //  Init lookaside list used to allocate our context structure used to
    //  pass information from out preOperation callback to our postOperation
    //  callback.    
    ExInitializeNPagedLookasideList(&Pre2PostContextList,
        NULL,
        NULL,
        0,
        sizeof(PRE_2_POST_CONTEXT),
        PRE_2_POST_TAG,
        0);
    
    //  Register with FltMgr    
    status = FltRegisterFilter(DriverObject,
        &FilterRegistration,
        &gFilterHandle);

    if (!NT_SUCCESS(status)) {

        goto SwapDriverEntryExit;
    }
    
    //  Start filtering i/o    
    status = FltStartFiltering(gFilterHandle);

    if (!NT_SUCCESS(status)) {

        FltUnregisterFilter(gFilterHandle);
        goto SwapDriverEntryExit;
    }

SwapDriverEntryExit:

    if (!NT_SUCCESS(status)) {

        ExDeleteNPagedLookasideList(&Pre2PostContextList);
    }

    return status;
}