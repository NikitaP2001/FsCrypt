#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>


// Pool Tags    
#define BUFFER_SWAP_TAG     'bdFL'
#define CONTEXT_TAG         'xcFL'
#define NAME_TAG            'mnFL'
#define PRE_2_POST_TAG      'ppFL'


//  This is a volume context, one of these are attached to each volume
//  we monitor.  This is used to get a "DOS" name for debug display.
typedef struct _VOLUME_CONTEXT {
    
    //  Holds the name to display    
    UNICODE_STRING Name;
    
    //  Holds the sector size for this volume.    
    ULONG SectorSize;

} VOLUME_CONTEXT, * PVOLUME_CONTEXT;

#define MIN_SECTOR_SIZE 0x200

//  This is a context structure that is used to pass state from our
//  pre-operation callback to our post-operation callback.
typedef struct _PRE_2_POST_CONTEXT {
    
    //  Pointer to our volume context structure.  We always get the context
    //  in the preOperation path because you can not safely get it at DPC
    //  level.  We then release it in the postOperation path.  It is safe
    //  to release contexts at DPC level.    

    PVOLUME_CONTEXT VolCtx;
    
    //  Since the post-operation parameters always receive the "original"
    //  parameters passed to the operation, we need to pass our new destination
    //  buffer to our post operation routine so we can free it.    

    PVOID SwappedBuffer;

} PRE_2_POST_CONTEXT, * PPRE_2_POST_CONTEXT;

extern NPAGED_LOOKASIDE_LIST Pre2PostContextList;


NTSTATUS
InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
CleanupVolumeContext(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
);

NTSTATUS
InstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
SwapPreReadBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
SwapPostReadBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
SwapPostReadBuffersWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
SwapPreDirCtrlBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
SwapPostDirCtrlBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
SwapPostDirCtrlBuffersWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
SwapPreWriteBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
SwapPostWriteBuffers(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);