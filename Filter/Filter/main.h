#ifndef _MAIN_H_
#define _MAIN_H_

extern PFLT_FILTER gFilterHandle;

#define CRYPT_KEY_SIZE 32
#define CRYPT_XOR_BUFFER_SIZE 0x400

struct STORAGE_ENTRY {
    LIST_ENTRY entry;

    UNICODE_STRING filePath;
};

struct CRYPT_STORAGE {

    UINT8 key[CRYPT_KEY_SIZE];

    struct STORAGE_ENTRY files;
};

extern struct CRYPT_STORAGE gCryptStorage;

VOID
CryptXorBuffer(_In_ struct CRYPT_STORAGE* storage,
    _In_ ULONG length,
    _In_ ULONGLONG offset,
    _Inout_ PUINT8 data);

BOOLEAN
CryptStorageIsFilePresent(_In_ struct CRYPT_STORAGE* storage, _In_ PFILE_OBJECT file);

BOOLEAN
CryptStorageIsAuthenticated(_In_ struct CRYPT_STORAGE* storage);

BOOLEAN
CryptStorageAuthenticate(_In_ struct CRYPT_STORAGE* storage, PUINT8 password);

VOID
CryptStorageDeauthenticate(_In_ struct CRYPT_STORAGE* storage);

NTSTATUS
CryptAddFile(_In_ struct CRYPT_STORAGE* storage,
             _In_ PCWSTR filePath,
             _In_ BOOLEAN newFile);

NTSTATUS
CryptRemoveFile(_In_ struct CRYPT_STORAGE* storage, _In_ PCWSTR filePath);

#define STORAGE_POOL 'fcst'

#define IS_UNICODE_STRING_EMPTY(us) ((us).Length == 0 || (us).Buffer == NULL)

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