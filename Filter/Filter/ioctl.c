#include <fltKernel.h>
#include <ntddk.h>

#include "ioctl.h"
#include "main.h"

#define NT_DEVICE_NAME      L"\\Device\\SIOCTL"
#define DOS_DEVICE_NAME     L"\\DosDevices\\FsCrypt"

VOID
SioctlUnloadDriver(_In_ PDRIVER_OBJECT DriverObject);

NTSTATUS
SioctlCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS
SioctlDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);


#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SioctlDeviceControl)
#pragma alloc_text( PAGE, IoctlDeviceInit)
#endif // ALLOC_PRAGMA


NTSTATUS
IoctlDeviceInit(_In_ PDRIVER_OBJECT DriverObject)
{    
    UNICODE_STRING  ntUnicodeString;
    UNICODE_STRING  ntWin32NameString;
    PDEVICE_OBJECT  deviceObject = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &ntUnicodeString,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {        
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = SioctlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SioctlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SioctlDeviceControl;
    DriverObject->DriverUnload = SioctlUnloadDriver;
   
    // Initialize a Unicode String containing the Win32 name
    // for our device.
    RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);
    
    // Create a symbolic link between our device name  and the Win32 name
    status = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

    if (!NT_SUCCESS(status)) {
        // Delete everything that this routine has allocated.        
        IoDeleteDevice(deviceObject);
    }
    return status;
}

/*
    This routine is called by the I/O system to unload the driver.
    Any resources previously allocated must be freed.

    DriverObject - a pointer to the object that represents our driver.
*/
VOID
SioctlUnloadDriver(_In_ PDRIVER_OBJECT DriverObject)
{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;

    PAGED_CODE();
    
    // Create counted string version of our Win32 device name.
    RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);

    
    // Delete the link from our device name to a name in the Win32 namespace.
    IoDeleteSymbolicLink(&uniWin32NameString);

    if (deviceObject != NULL) {
        IoDeleteDevice(deviceObject);
    }
}

/*
    This routine is called by the I/O system when the SIOCTL is opened or
    closed.
    No action is performed other than completing the request successfully.

    @DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.
    @Irp - a pointer to the I/O Request Packet for this request.

Return Value:
    NT status code
*/
NTSTATUS
SioctlCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

static
BOOLEAN
SioctlIsPathValid(PCWSTR path, ULONG length)
{
    ULONG pos = 0;

    if (length == 0)
        return FALSE;

    while (pos * sizeof(WCHAR) < length) {
        if (path[pos++] == 0)
            return TRUE;        
    }
    return FALSE;
}

/*
    This routine is called by the I/O system to perform a device I/O
    control function.

    DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.
Return Value:
    NT status code
*/
NTSTATUS
SioctlDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    IOCTL_RESPONSE ioResp = { .type = IoctlSuccess };
    PIO_STACK_LOCATION  irpSp;
    NTSTATUS            status = STATUS_SUCCESS;
    ULONG               inBufLength;
    ULONG               outBufLength;
    PCHAR               inBuf;        
    PCHAR               buffer = NULL;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (!inBufLength || outBufLength != sizeof(IOCTL_RESPONSE)) {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }
    
    if (irpSp->Parameters.DeviceIoControl.IoControlCode
        != IOCTL_SIOCTL_METHOD_OUT_DIRECT) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto End;
    }        
    

    inBuf = Irp->AssociatedIrp.SystemBuffer;

    buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority | MdlMappingNoExecute);

    if (!buffer) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    if (inBufLength >= sizeof(IOCTL_COMMAND)) {
        PIOCTL_COMMAND pCmd = (PIOCTL_COMMAND)inBuf;
        PVOID pData = inBuf + sizeof(IOCTL_COMMAND);
        ULONG length = pCmd->dataLen;
        switch (pCmd->type) {
        case IoctlAddFile:
            if (!CryptStorageIsAuthenticated(&gCryptStorage)) {
                ioResp.type = IoctlNotAuth;
                break;
            }
                
            if (length > 0 && inBufLength >= sizeof(IOCTL_COMMAND) 
                + length && SioctlIsPathValid(pData, length))
                status = CryptAddFile(&gCryptStorage, pData, FALSE);
            else
                status = STATUS_INVALID_DEVICE_REQUEST;                                    
            break;
        case IoctlAddNewFile:

            if (!CryptStorageIsAuthenticated(&gCryptStorage)) {
                ioResp.type = IoctlNotAuth;
                break;
            }

            if (length > 0 && inBufLength >= sizeof(IOCTL_COMMAND)
                + length && SioctlIsPathValid(pData, length))
                status = CryptAddFile(&gCryptStorage, pData, TRUE);
            else
                status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        case IoctlRemoveFile:

            if (!CryptStorageIsAuthenticated(&gCryptStorage)) {
                ioResp.type = IoctlNotAuth;
                break;
            }

            if (length > 0 && inBufLength >= sizeof(IOCTL_COMMAND)
                + length && SioctlIsPathValid(pData, length))
                status = CryptRemoveFile(&gCryptStorage, pData);
            else
                status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        case IoctlSetPassword:
            if (length != CRYPT_KEY_SIZE || inBufLength < length) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (!CryptStorageAuthenticate(&gCryptStorage, pData)) {
                ioResp.type = IoctlInvalidPassword;
                break;
            }                                        
            break;
        case IoctlDeauth:
            if (!CryptStorageIsAuthenticated(&gCryptStorage)) {
                ioResp.type = IoctlNotAuth;
                break;
            }

            CryptStorageDeauthenticate(&gCryptStorage);
            break;
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }     

    RtlCopyBytes(buffer, &ioResp, sizeof(IOCTL_RESPONSE));
        
    Irp->IoStatus.Information = sizeof(IOCTL_RESPONSE);        
End:    

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}