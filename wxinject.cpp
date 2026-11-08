// wxinject.cpp — 微信 4.1.15.56 hook 加载器
// 用法: wxinject.exe [Weixin.exe 路径] [version.dll 路径]
// 流程: CREATE_SUSPENDED 启动微信 -> 轮询等待 Weixin.dll 加载 -> 远线程 LoadLibraryW 注入 -> 放行主线程
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>

static bool ModuleInProcess(DWORD pid, const wchar_t* module)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, module) == 0) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

int wmain(int argc, wchar_t** argv)
{
    const wchar_t* wxPath = (argc > 1) ? argv[1] : L"D:\\Program Files\\Tencent\\Weixin\\Weixin.exe";
    const wchar_t* dllPath = (argc > 2) ? argv[2] : L"D:\\Program Files\\Tencent\\Weixin\\version.dll";

    wchar_t cmd[1024];
    swprintf_s(cmd, L"\"%s\"", wxPath);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(wxPath, cmd, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        wprintf(L"[injector] CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }
    wprintf(L"[injector] Weixin.exe started suspended, pid=%lu\n", pi.dwProcessId);

    // 等主进程加载 Weixin.dll（hook 的 Evt_WeixinLoad 需要 Weixin.dll 已加载）
    for (int i = 0; i < 600; i++) {   // 最多 60s
        if (ModuleInProcess(pi.dwProcessId, L"Weixin.dll"))
            break;
        // 进程退出则报错
        DWORD ec = 0;
        if (GetExitCodeProcess(pi.hProcess, &ec) && ec != STILL_ACTIVE) {
            wprintf(L"[injector] Weixin.exe exited early, code=%lu\n", ec);
            return 2;
        }
        Sleep(100);
        if (i == 0) {   // 首次等待前先放行主线程，否则微信可能停在内部初始化等待上
            ResumeThread(pi.hThread);
            wprintf(L"[injector] main thread resumed, waiting for Weixin.dll...\n");
        }
    }
    if (!ModuleInProcess(pi.dwProcessId, L"Weixin.dll")) {
        wprintf(L"[injector] timeout waiting for Weixin.dll\n");
        ResumeThread(pi.hThread);
        return 3;
    }

    SIZE_T pathBytes = (lstrlenW(dllPath) + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(pi.hProcess, nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        wprintf(L"[injector] VirtualAllocEx failed: %lu\n", GetLastError());
        return 4;
    }
    if (!WriteProcessMemory(pi.hProcess, remote, dllPath, pathBytes, nullptr)) {
        wprintf(L"[injector] WriteProcessMemory failed: %lu\n", GetLastError());
        return 5;
    }

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(pi.hProcess, nullptr, 0, pLoadLibraryW, remote, 0, nullptr);
    if (!hThread) {
        wprintf(L"[injector] CreateRemoteThread failed: %lu\n", GetLastError());
        return 6;
    }
    WaitForSingleObject(hThread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);

    if (!ModuleInProcess(pi.dwProcessId, L"version.dll")) {
        wprintf(L"[injector] LoadLibraryW returned 0x%08lx — hook dll not resident\n", exitCode);
        return 7;
    }
    wprintf(L"[injector] hook dll injected OK, pid=%lu\n", pi.dwProcessId);
    return 0;
}
