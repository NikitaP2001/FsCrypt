#include <filesystem>
#include <iostream>
#include <cassert>

#include <windows.h>

constexpr static TCHAR g_driverName[] = TEXT("Filter.sys");
constexpr static TCHAR g_driverDescr[] = TEXT("Filesystem encryption filter driver");

namespace fs = std::filesystem;


int main()
{
        SERVICE_STATUS svStat;
        BOOL status;
        int c;
        fs::path binaryPath = fs::absolute(g_driverName);
        SC_HANDLE hMngr = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        assert(hMngr != NULL);

        std::wcout << binaryPath.native().c_str() << std::endl;


        SC_HANDLE hService = OpenService(hMngr, g_driverName, SERVICE_ALL_ACCESS);
        if (hService == nullptr && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
                std::cout << "Service has been created" << std::endl;
                hService = CreateService(hMngr,
                                        g_driverName,
                                        g_driverDescr,
                                        SERVICE_ALL_ACCESS,
                                        SERVICE_KERNEL_DRIVER,
                                        SERVICE_DEMAND_START,
                                        SERVICE_ERROR_IGNORE,
                                        binaryPath.native().c_str(),
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr);
        }
        std::cout << GetLastError() << std::endl;
        assert(hService != nullptr);

        std::cout << "Start the service y/n>";
        c = getchar();
        if (c == 'y') {
                status = StartService(hService, 0, NULL);
                if (status != false) {
                        std::cout << "Stop service y/n>";
                        c = getchar();
                        if (c == 'y') {
                                status = ControlService(hService, SERVICE_CONTROL_STOP, &svStat);
                                assert(status);
                        }
                } else {
                        std::cout << "Sv start: " << GetLastError() << std::endl;
                }
        }

        std::cout << "Delete service y/n>";
        c = getchar();
        if (c == 'y')
                DeleteService(hService);

        CloseServiceHandle(hService);

        CloseServiceHandle(hMngr);
}