#ifndef _MAIN_H_
#define _MAIN_H_
extern PFLT_FILTER gFilterHandle;

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);
#endif /* _MAIN_H_ */