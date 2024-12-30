#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <iostream>

std::vector<std::wstring> ListProcesses() {
    std::vector<std::wstring> processList;
    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) return processList;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            processList.push_back(entry.szExeFile);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processList;
}

bool InjectDLL(const std::wstring& processName, const std::wstring& dllPath) {
    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    HANDLE hProcess = nullptr;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    if (!hProcess) return false;

    LPVOID allocMem = VirtualAllocEx(hProcess, nullptr, (dllPath.size() + 1) * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocMem) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, allocMem, dllPath.c_str(), (dllPath.size() + 1) * sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, allocMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

void DisplayMain() {
    std::wcout << L"                                       \n";
    std::wcout << L"     _    ____ ___                     \n";
    std::wcout << L"    / \\  / ___/ _ \\                   \n";
    std::wcout << L"   / _ \\| |  | (_) |                  \n";
    std::wcout << L"  / ___ \\ |___\\__, |                 \n";
    std::wcout << L" /_/   \\_\\____| /_/                  \n";
    std::wcout << L"                                       \n";
}

void DisplayMenu() {
    std::wcout << L"Select an option:\n";
    std::wcout << L"1. Manual Injection (specify game and DLL path)\n";
    std::wcout << L"2. Process Injection (choose process and specify DLL path)\n";
    std::wcout << L"3. Auto Injection (automatically inject DLL into chosen process)\n";
    std::wcout << L"4. Change Logs\n";
    std::wcout << L"5. Settings\n";
}

int wmain() {
    DisplayMain();
    DisplayMenu();

    int choice;
    std::wcin >> choice;

    switch (choice) {
    case 1: {
        std::wstring gamePath, dllPath;
        std::wcin.ignore();
        std::wcout << L"Enter game executable path: ";
        std::getline(std::wcin, gamePath);
        std::wcout << L"Enter DLL path: ";
        std::getline(std::wcin, dllPath);
        std::wcout << L"Manual Injection chosen. Injecting...\n";
        break;
    }
    case 2: {
        std::vector<std::wstring> processes = ListProcesses();
        if (processes.empty()) {
            std::wcout << L"No processes found. Exiting...\n";
            break;
        }

        std::wcout << L"Running processes:\n";
        for (size_t i = 0; i < processes.size(); ++i) {
            std::wcout << i + 1 << L". " << processes[i] << L"\n";
        }

        size_t processChoice;
        std::wcout << L"Select the process number for injection: ";
        std::wcin >> processChoice;

        if (processChoice < 1 || processChoice > processes.size()) {
            std::wcout << L"Invalid choice. Exiting...\n";
            break;
        }

        std::wstring dllPath;
        std::wcout << L"Enter the DLL path: ";
        std::wcin.ignore();
        std::getline(std::wcin, dllPath);

        if (InjectDLL(processes[processChoice - 1], dllPath)) {
            std::wcout << L"DLL injected successfully!\n";
        }
        else {
            std::wcout << L"Failed to inject DLL.\n";
        }
        break;
    }
    case 3: {
        std::wcout << L"Auto Injection is not implemented yet. Stay tuned!\n";
        break;
    }
    case 4:
        std::wcout << L"Displaying change logs...\n";
        break;
    case 5:
        std::wcout << L"Settings menu...\n";
        break;
    default:
        std::wcout << L"Invalid choice. Exiting...\n";
        break;
    }

    return 0;
}
