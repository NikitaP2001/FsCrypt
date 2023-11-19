
#define SIOCTL_TYPE 40000


#define IOCTL_SIOCTL_METHOD_IN_DIRECT \
    CTL_CODE( SIOCTL_TYPE, 0x900, METHOD_IN_DIRECT, FILE_ANY_ACCESS  )

#define IOCTL_SIOCTL_METHOD_OUT_DIRECT \
    CTL_CODE( SIOCTL_TYPE, 0x901, METHOD_OUT_DIRECT , FILE_ANY_ACCESS  )


#ifdef _KERNEL_MODE

NTSTATUS
IoctlDeviceInit(_In_ PDRIVER_OBJECT DriverObject);

#endif

enum IoctlCommandType {
    IoctlAddFile,
    IoctlAddNewFile,
    IoctlSetPassword,
};

typedef struct IOCTL_COMMAND {
    ULONG   type;
    ULONG   dataLen;
} IOCTL_COMMAND, *PIOCTL_COMMAND;

enum IoctlResponseType {
    IoctlSuccess,
    IoctlNotAuth,
    IoctlInvalidPassword,
    IoctlDeauth,
    IoctlRemoveFile
};

typedef struct IOCTL_RESPONSE {
    ULONG   type;   
} IOCTL_RESPONSE, *PIOCTL_RESPONSE;

#define IOCTL_DRIVER_NAME "\\\\.\\FsCrypt"