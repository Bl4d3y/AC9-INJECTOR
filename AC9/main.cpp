#include <windows.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx9.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct ProcessInfo {
    DWORD processID;
    std::wstring processName;
};

HWND hwnd;
IDirect3D9* d3d = nullptr;
IDirect3DDevice9* d3dDevice = nullptr;
std::vector<ProcessInfo> processList;
DWORD selectedProcessID = 0;
std::wstring selectedDLL;

void RefreshProcessList() {
    processList.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snap, &pe)) {
        do {
            processList.push_back({ pe.th32ProcessID, pe.szExeFile });
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

bool InjectDLL(const std::wstring& dllPath, DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!hProcess) return false;

    void* allocMem = VirtualAllocEx(hProcess, nullptr, (dllPath.size() + 1) * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocMem) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, allocMem, dllPath.c_str(), (dllPath.size() + 1) * sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW"), allocMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}

std::wstring OpenFileDialog() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Dynamic Link Library (*.dll)\0*.dll\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        return fileName;
    }
    return L"";
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void RenderUI() {
    ImGui::Begin("AC9 Injector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Button("Refresh Process List")) RefreshProcessList();

    ImGui::Text("Processes:");
    if (ImGui::BeginListBox("##processList", ImVec2(300, 150))) {
        for (const auto& proc : processList) {
            if (ImGui::Selectable((std::to_string(proc.processID) + " [" + std::string(proc.processName.begin(), proc.processName.end()) + "]").c_str(), selectedProcessID == proc.processID)) {
                selectedProcessID = proc.processID;
            }
        }
        ImGui::EndListBox();
    }

    if (ImGui::Button("Select DLL")) {
        selectedDLL = OpenFileDialog();
    }
    ImGui::SameLine();
    ImGui::Text("%ls", selectedDLL.c_str());

    if (ImGui::Button("Inject")) {
        if (selectedProcessID != 0 && !selectedDLL.empty()) {
            if (InjectDLL(selectedDLL, selectedProcessID)) {
                MessageBox(hwnd, L"DLL injected successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
            }
            else {
                MessageBox(hwnd, L"Failed to inject DLL!", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        else {
            MessageBox(hwnd, L"Please select a process and a DLL!", L"Error", MB_OK | MB_ICONERROR);
        }
    }

    ImGui::End();
}

void Cleanup() {
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (d3dDevice) d3dDevice->Release();
    if (d3d) d3d->Release();
    DestroyWindow(hwnd);
}

void InitD3D(HWND hWnd) {
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3dDevice);
}

void SetTransparentAndFullscreen(HWND hWnd) {
    SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);
    SetWindowLong(hWnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOPMOST);
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY); 

    // Get screen dimensions
    RECT screenRect;
    GetClientRect(GetDesktopWindow(), &screenRect);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, screenRect.right, screenRect.bottom, SWP_SHOWWINDOW);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"AC9 Injector", NULL };
    RegisterClassEx(&wc);

    hwnd = CreateWindowEx(0, wc.lpszClassName, L"AC9 Injector", WS_POPUP, 0, 0, 1920, 1080, NULL, NULL, wc.hInstance, NULL);

    InitD3D(hwnd);
    SetTransparentAndFullscreen(hwnd);

    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(d3dDevice);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    RefreshProcessList();

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0); // Transparent background
        d3dDevice->BeginScene();

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

        d3dDevice->EndScene();
        d3dDevice->Present(NULL, NULL, NULL, NULL);
    }

    Cleanup();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
