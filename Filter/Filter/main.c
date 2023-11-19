#include "filter.h"
#include "FltDbg.h"
#include "ioctl.h"
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

struct CRYPT_STORAGE gCryptStorage;

VOID
CryptStorageInit(_Inout_ struct CRYPT_STORAGE *storage)
{
    InitializeListHead(&storage->files.entry);
    CONST UINT8 temp_key[CRYPT_KEY_SIZE] = {0};

    RtlCopyMemory(storage->key, temp_key, CRYPT_KEY_SIZE);
}

VOID
CryptXorBuffer(_In_ struct CRYPT_STORAGE* storage,
               _In_ ULONG length,
               _In_ ULONGLONG offset,
               _Inout_ PUINT8 data)
{    
    for (ULONG i_data = 0; i_data < length; i_data++) {
        offset %= CRYPT_KEY_SIZE;
        data[i_data] ^= storage->key[offset++];
    }
}

static
UNICODE_STRING 
AllocateAndCopyUnicodeString(PCUNICODE_STRING source)
{
    UNICODE_STRING destination;
    
    RtlInitUnicodeString(&destination, NULL);
    
    destination.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_NON_PAGED, 
        source->Length, STORAGE_POOL);

    if (destination.Buffer != NULL) {        
        destination.MaximumLength = source->Length;
        RtlCopyUnicodeString(&destination, source);
    } else {        
        destination.Length = 0;
    }

    return destination;
}

struct STORAGE_ENTRY*
StorageEntryCreate(PCUNICODE_STRING filePath)
{
    UNICODE_STRING path;
    struct STORAGE_ENTRY* entry = ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                                  sizeof(struct STORAGE_ENTRY), 
                                                  'fcst');
    if (entry != NULL) {
        path = AllocateAndCopyUnicodeString(filePath);
        if (IS_UNICODE_STRING_EMPTY(path)) {
            ExFreePool(entry);
            entry = NULL;
        } else
            entry->filePath = path;
    }    
    return entry;
}

VOID
StorageEntryDestroy(_In_  struct STORAGE_ENTRY* entry)
{
    if (entry->filePath.Buffer != NULL)
        ExFreePool(entry->filePath.Buffer);
    ExFreePool(entry);
}

static
NTSTATUS
CryptStorageAddFile(_In_ struct CRYPT_STORAGE* storage, _In_ PFILE_OBJECT file)
{   
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (!IS_UNICODE_STRING_EMPTY(file->FileName)) {
        struct STORAGE_ENTRY* sentry = StorageEntryCreate(&file->FileName);
        if (sentry != NULL) {
            InsertTailList(&storage->files.entry, &sentry->entry);
            status = STATUS_SUCCESS;
        }            
    }
    return status;
}

static
NTSTATUS
CryptStorageRemoveFile(_In_ struct CRYPT_STORAGE* storage, _In_ PFILE_OBJECT file)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;    
    PLIST_ENTRY head = &storage->files.entry;    

    if (IS_UNICODE_STRING_EMPTY(file->FileName))
        return status;

    for (PLIST_ENTRY curr = head->Flink;
        curr != head;
        curr = curr->Flink) {
        struct STORAGE_ENTRY* entry = CONTAINING_RECORD(curr, struct STORAGE_ENTRY, entry);
        if (RtlEqualUnicodeString(&entry->filePath, &file->FileName, TRUE)) {
            if (RemoveEntryList(curr)) {
                StorageEntryDestroy(entry);
                status = STATUS_SUCCESS;
            }            
        }
    }    
    return status;
}

BOOLEAN
CryptStorageIsAuthenticated(_In_ struct CRYPT_STORAGE* storage)
{    
    for (ULONG i = 0; i < CRYPT_KEY_SIZE; i++) {
        if (storage->key[i] != 0)
            return TRUE;
    }
    return FALSE;    
}

BOOLEAN
CryptStorageAuthenticate(_In_ struct CRYPT_STORAGE* storage, PUINT8 password)
{    
    if (!RtlIsZeroMemory(password, CRYPT_KEY_SIZE)) {
        RtlCopyMemory(storage->key, password, CRYPT_KEY_SIZE);
        return TRUE;
    }
    return FALSE;
}

VOID
CryptStorageDeauthenticate(_In_ struct CRYPT_STORAGE* storage)
{
    RtlZeroMemory(storage->key, CRYPT_KEY_SIZE);
}

BOOLEAN
CryptStorageIsFilePresent(_In_ struct CRYPT_STORAGE* storage, _In_ PFILE_OBJECT file)
{
    PLIST_ENTRY head = &storage->files.entry;
    BOOLEAN result = FALSE;

    if (IS_UNICODE_STRING_EMPTY(file->FileName))
        return result;

    for (PLIST_ENTRY curr = head->Flink;
         curr != head && !result; 
         curr = curr->Flink) {
        struct STORAGE_ENTRY* entry = CONTAINING_RECORD(curr, struct STORAGE_ENTRY, entry);
        result = RtlEqualUnicodeString(&entry->filePath, &file->FileName, TRUE);        
    }
    return result;
}

VOID
CryptStorageFree(struct CRYPT_STORAGE* storage)
{
    PLIST_ENTRY head = &storage->files.entry;

    while (!IsListEmpty(head)) {
        struct STORAGE_ENTRY* entry = CONTAINING_RECORD(RemoveHeadList(head), 
                                                        struct STORAGE_ENTRY, 
                                                        entry);
        StorageEntryDestroy(entry);
    }    
}

static
NTSTATUS
CryptXorFileContent(_In_ struct CRYPT_STORAGE* storage, _In_ HANDLE fileHandle)
{       
    IO_STATUS_BLOCK ioStatusBlock;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UINT8 buffer[CRYPT_XOR_BUFFER_SIZE];
    LARGE_INTEGER offset = { .QuadPart = 0 };

    do {
        status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock,
            buffer, CRYPT_XOR_BUFFER_SIZE, &offset, NULL);

        if (NT_SUCCESS(status) && ioStatusBlock.Information > 0) {

            CryptXorBuffer(storage, CRYPT_XOR_BUFFER_SIZE, offset.QuadPart, buffer);
            ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock,
                buffer, (ULONG)ioStatusBlock.Information, &offset, NULL);
            offset.QuadPart += ioStatusBlock.Information;
        }
        else {
            break;
        }
    } while (NT_SUCCESS(status));

    if (status == STATUS_END_OF_FILE)
        status = STATUS_SUCCESS;

    return status;
}

NTSTATUS
CryptRemoveFile(_In_ struct CRYPT_STORAGE* storage, _In_ PCWSTR filePath)
{
    UNICODE_STRING unicodeFilePath;
    OBJECT_ATTRIBUTES objectAttributes;
    PFILE_OBJECT fileObject = NULL;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    RtlInitUnicodeString(&unicodeFilePath, filePath);

    InitializeObjectAttributes(&objectAttributes,
        &unicodeFilePath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        0,
        NULL);

    status = ZwOpenFile(&fileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        0,
        FILE_SYNCHRONOUS_IO_NONALERT);
    if (NT_SUCCESS(status)) {

        status = ObReferenceObjectByHandle(
            fileHandle,
            0,
            *IoFileObjectType,
            KernelMode,
            (PVOID*)&fileObject,
            NULL
        );

        if (NT_SUCCESS(status)) {
            BOOLEAN isExist = CryptStorageIsFilePresent(storage, fileObject);

            if (isExist)
                status = CryptXorFileContent(storage, fileHandle);

            if (NT_SUCCESS(status) && isExist)
                CryptStorageRemoveFile(storage, fileObject);

            ObDereferenceObject(fileObject);
        }

        ZwClose(fileHandle);
    }
    return status;
}

NTSTATUS
CryptAddFile(_In_ struct CRYPT_STORAGE* storage, 
             _In_ PCWSTR filePath, 
             _In_ BOOLEAN newFile)
{
    UNICODE_STRING unicodeFilePath;
    OBJECT_ATTRIBUTES objectAttributes;
    PFILE_OBJECT fileObject = NULL;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE fileHandle = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    RtlInitUnicodeString(&unicodeFilePath, filePath);

    InitializeObjectAttributes(&objectAttributes,
        &unicodeFilePath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        0,
        NULL);

    status = ZwOpenFile(&fileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        0,
        FILE_SYNCHRONOUS_IO_NONALERT);
    if (NT_SUCCESS(status)) {

        status = ObReferenceObjectByHandle(
            fileHandle,
            0,
            *IoFileObjectType,
            KernelMode,
            (PVOID*)&fileObject,
            NULL
        );

        if (NT_SUCCESS(status)) {
            BOOLEAN isExist = CryptStorageIsFilePresent(storage, fileObject);

            if (newFile && !isExist)
                status = CryptXorFileContent(storage, fileHandle);

            if (NT_SUCCESS(status) && !isExist)
                CryptStorageAddFile(storage, fileObject);

            ObDereferenceObject(fileObject);
        }        


        ZwClose(fileHandle);
    }
    return status;
}

NTSTATUS
FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Flags);

    CryptStorageFree(&gCryptStorage);

    //  Unregister from FLT mgr    
    FltUnregisterFilter(gFilterHandle);

    //  Delete lookaside list    
    ExDeleteNPagedLookasideList(&Pre2PostContextList);

    return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
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

    CryptStorageInit(&gCryptStorage);

    status = IoctlDeviceInit(DriverObject);
    if (!NT_SUCCESS(status))
        goto SwapDriverEntryExit;
    
    //  Register with FltMgr    
    status = FltRegisterFilter(DriverObject,
        &FilterRegistration,
        &gFilterHandle);

    if (!NT_SUCCESS(status)) {

        goto SwapDriverEntryExit;
    }
        
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