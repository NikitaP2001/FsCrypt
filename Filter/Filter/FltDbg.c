#include <wdm.h>

#include "FltDbg.h"

ULONG LoggingFlags = 0;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, ReadDriverParameters)
#endif

VOID
ReadDriverParameters(_In_ PUNICODE_STRING RegistryPath)
/*
 * This routine tries to read the driver-specific parameters from
 * the registry.  These values will be found in the registry location
 * indicated by the RegistryPath passed in.
 * RegistryPath - the path key passed to the driver during driver entry.
 */
{
    OBJECT_ATTRIBUTES attributes;
    HANDLE driverRegKey;
    NTSTATUS status;
    ULONG resultLength;
    UNICODE_STRING valueName;
    UCHAR buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(LONG)];

    //  If this value is not zero then somebody has already explicitly set it
    //  so don't override those settings.    
    if (0 == LoggingFlags) {

        //  Open the desired registry key        
        InitializeObjectAttributes(&attributes,
            RegistryPath,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            NULL);

        status = ZwOpenKey(&driverRegKey,
            KEY_READ,
            &attributes);

        if (!NT_SUCCESS(status)) {

            return;
        }

        // Read the given value from the registry.        
        RtlInitUnicodeString(&valueName, L"DebugFlags");

        status = ZwQueryValueKey(driverRegKey,
            &valueName,
            KeyValuePartialInformation,
            buffer,
            sizeof(buffer),
            &resultLength);

        if (NT_SUCCESS(status)) {

            LoggingFlags = *((PULONG) & (((PKEY_VALUE_PARTIAL_INFORMATION)buffer)->Data));
        }

        //  Close the registry entry        
        ZwClose(driverRegKey);
    }
}
