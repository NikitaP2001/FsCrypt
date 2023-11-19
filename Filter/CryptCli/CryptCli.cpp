#include <stdio.h>

#include <windows.h>
#include <tchar.h>

#include "json.hpp"

#include <ioctl.h>
#include "sha256.h"

#define CPRINTF(...) _tprintf(__VA_ARGS__);

#define NTPATH_PREFIX _T("\\??\\Global\\")

static 
BOOL
IoctlSend(_In_ PUINT8 inputBuffer, _In_ DWORD inSize, _Out_ PIOCTL_RESPONSE resp)
{
    HANDLE hDev;   
    DWORD bRet;
    BOOL rc;

    hDev = CreateFileA(IOCTL_DRIVER_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    memset(resp, 0, sizeof(IOCTL_RESPONSE));
    rc = DeviceIoControl(hDev,
        (DWORD)IOCTL_SIOCTL_METHOD_OUT_DIRECT,
        inputBuffer,
        inSize,
        resp,
        sizeof(IOCTL_RESPONSE),
        &bRet,
        NULL
    );

    if (bRet != sizeof(IOCTL_RESPONSE))
        rc = FALSE;    

    CloseHandle(hDev);
    return rc;
}

static
BOOL
AddPassword(_In_ PTCHAR password, _Out_ PIOCTL_RESPONSE resp)
{
    IOCTL_COMMAND cmd;   
    BOOL result = FALSE;
    ULONG msgLen = sizeof(IOCTL_COMMAND) + SHA256_KEYSIZE;
    PUINT8 passBuffer;
    struct sha256_state stSha;


    ULONG passLen = _tcslen(password) * sizeof(TCHAR);
    if (passLen % SHA256_BLKSIZE != 0 || passLen == 0)
        passLen += (SHA256_BLKSIZE - passLen % SHA256_BLKSIZE);
    CPRINTF(_T("Pass len is %d"), passLen);
    passBuffer = reinterpret_cast<PUINT8>(malloc(passLen));
    if (passBuffer == NULL)
        return result;
    memcpy(passBuffer, password, _tcslen(password) * sizeof(TCHAR));

    sha256_state_init(&stSha);
    sha256_process_x86(&stSha, passBuffer, passLen);

    memset(resp, 0, sizeof(IOCTL_RESPONSE));
                       
    CPRINTF(_T("Authentication with password: : %s\n"), password);
    PUINT8 message = reinterpret_cast<PUINT8>(malloc(msgLen));
    if (message != NULL) {
        cmd.type = IoctlSetPassword;
        cmd.dataLen = SHA256_KEYSIZE;
        memcpy(message, &cmd, sizeof(IOCTL_COMMAND));
        memcpy(message + sizeof(IOCTL_COMMAND), stSha.hash_data, SHA256_KEYSIZE);
        result = IoctlSend(message, msgLen, resp);
        free(message);
    }   
    free(passBuffer);
    return result;
}

static
BOOL
PathCmd(_In_ PTCHAR path, _In_ PIOCTL_COMMAND cmd, _Out_ PIOCTL_RESPONSE resp)
{
    BOOL result = FALSE;
    DWORD length = GetFullPathName(path, 0, NULL, NULL);

    memset(resp, 0, sizeof(IOCTL_RESPONSE));

    if (length != 0) {
        DWORD fullLen = length * sizeof(TCHAR) + sizeof(NTPATH_PREFIX);
        LPTSTR fullPath = reinterpret_cast<LPTSTR>(malloc(fullLen));
        LPTSTR dosPath = (LPTSTR)((UINT8*)fullPath + sizeof(NTPATH_PREFIX) - sizeof(TCHAR));
        if (GetFullPathName(path, length, dosPath, NULL)) {
            memcpy(fullPath, NTPATH_PREFIX, sizeof(NTPATH_PREFIX) - sizeof(TCHAR));
            PUINT8 finStruct = reinterpret_cast<PUINT8>(malloc(sizeof(IOCTL_COMMAND) + fullLen));
            if (finStruct != NULL) {
                CPRINTF(_T("File: %s\n"), fullPath);
                cmd->dataLen = fullLen;
                memcpy(finStruct, cmd, sizeof(IOCTL_COMMAND));
                memcpy(finStruct + sizeof(IOCTL_COMMAND), fullPath, fullLen);
                result = IoctlSend(finStruct, sizeof(IOCTL_COMMAND) + fullLen, resp);
                free(finStruct);
                free(fullPath);
            }
        }
    }
    return result;
}

static
BOOL
AddNewFile(_In_ PTCHAR path, _Out_ PIOCTL_RESPONSE resp)
{
    IOCTL_COMMAND cmd;
    cmd.type = IoctlAddNewFile;

    return PathCmd(path, &cmd, resp);
}

static
BOOL
AddFile(_In_ PTCHAR path, _Out_ PIOCTL_RESPONSE resp)
{
    IOCTL_COMMAND cmd;    
    cmd.type = IoctlAddFile;

    return PathCmd(path, &cmd, resp);
}

static
BOOL
RemoveFile(_In_ PTCHAR path, _Out_ PIOCTL_RESPONSE resp)
{
    IOCTL_COMMAND cmd;
    cmd.type = IoctlRemoveFile;

    return PathCmd(path, &cmd, resp);
}

static
BOOL
Deauthenticate(_Out_ PIOCTL_RESPONSE resp)
{
    IOCTL_COMMAND cmd;
    BOOL result = FALSE;

    memset(resp, 0, sizeof(IOCTL_RESPONSE));

    cmd.type = IoctlDeauth;
    cmd.dataLen = 0;    
    result = IoctlSend(reinterpret_cast<PUINT8>(&cmd), sizeof(IOCTL_COMMAND), resp);
             
    return result;
}

int _tmain(int argc, TCHAR *argv[])
{    
    IOCTL_RESPONSE resp = { 0 };
    BOOL status = FALSE;

    for (int i = 0; i < argc; i++) {
        if (i + 1 < argc && _tcscmp(argv[i], _T("add")) == 0) {            
            status = AddFile(argv[i + 1], &resp);
            break;
        }
        if (i + 1 < argc && _tcscmp(argv[i], _T("password")) == 0) {
            status = AddPassword(argv[i + 1], &resp);
            break;
        }
        if (i + 1 < argc && _tcscmp(argv[i], _T("addn")) == 0) {
            status = AddNewFile(argv[i + 1], &resp);
            break;
        }
        if (i + 1 < argc && _tcscmp(argv[i], _T("remove")) == 0) {
            status = RemoveFile(argv[i + 1], &resp);
            break;
        }
        if (_tcscmp(argv[i], _T("deauth")) == 0) {
            status = Deauthenticate(&resp);
            break;
        }
    }  

    if (!status || resp.type != IoctlSuccess) {
        _tprintf(_T("Operation Failed\n"));
        PCTSTR reason = NULL;
        switch (resp.type) {
        case IoctlSuccess:
            reason = _T("reason unknown");
            break;
        case IoctlNotAuth:
            reason = _T("not authenticated");
            break;
        case IoctlInvalidPassword:
            reason = _T("wrong password format");
            break;
        }
        if (reason != NULL) {
            _tprintf(_T("Reason: %s\n"), reason);
        } else {
            _tprintf(_T("Reason unknown\n"));
        }
    } else {
        _tprintf(_T("Operation completed OK\n"));
    }
}
