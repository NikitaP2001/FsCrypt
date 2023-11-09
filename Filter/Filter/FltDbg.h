#ifndef _DBG_H_
#define _DBG_H_

//
//  Definitions to display log messages.  The registry DWORD entry:
//  "hklm\system\CurrentControlSet\Services\Swapbuffers\DebugFlags" defines
//  the default state of these logging flags
//

#define LOGFL_ERRORS    0x00000001  // if set, display error messages
#define LOGFL_READ      0x00000002  // if set, display READ operation info
#define LOGFL_WRITE     0x00000004  // if set, display WRITE operation info
#define LOGFL_DIRCTRL   0x00000008  // if set, display DIRCTRL operation info
#define LOGFL_VOLCTX    0x00000010  // if set, display VOLCTX operation info

extern ULONG LoggingFlags;

#define LOG_PRINT( _logFlag, _string )                              \
    (FlagOn(LoggingFlags,(_logFlag)) ?                              \
        DbgPrint  _string  :                                        \
        ((int)0))

VOID
ReadDriverParameters(_In_ PUNICODE_STRING RegistryPath);


#endif /* _DBG_H_ */