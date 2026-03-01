/*
 * Windows 10 Optimizer
 * Restores system responsiveness when Windows becomes slow after extended use.
 * Run as Administrator for full functionality.
 *
 * Optimizations performed:
 * - Clear temp files (TEMP, TMP, Windows\Temp)
 * - Empty Recycle Bin
 * - Flush DNS cache
 * - Restart Windows Explorer (fixes sluggish desktop/taskbar)
 * - Optimize screen/display (flush graphics, redraw desktop, restart DWM when lagging)
 * - Flush filesystem cache (volume C:), clear clipboard
 * - Boost process priority, trim current process working set
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Result buffer for confirmation message (shared) */
#define RESULT_BUF_SIZE 2048
static WCHAR g_resultBuf[RESULT_BUF_SIZE];

static void AppendResult(const WCHAR* text) {
    size_t curLen = wcslen(g_resultBuf);
    size_t addLen = wcslen(text);
    if (curLen > 0) addLen++;  /* newline */
    if (curLen + addLen + 1 < RESULT_BUF_SIZE) {
        if (curLen > 0) wcscat(g_resultBuf, L"\n");
        wcscat(g_resultBuf, text);
    }
}

/* Delete all files in a directory (non-recursive) */
static void ClearDirectory(const WCHAR* path) {
    WCHAR searchPath[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    size_t pathLen = wcslen(path);

    if (pathLen >= MAX_PATH - 4) return;

    wcscpy(searchPath, path);
    if (searchPath[pathLen - 1] != L'\\') {
        wcscat(searchPath, L"\\");
        pathLen++;
    }
    wcscpy(searchPath + pathLen, L"*");

    hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        WCHAR fullPath[MAX_PATH];
        wcscpy(fullPath, path);
        if (pathLen > 0 && fullPath[pathLen - 1] != L'\\')
            wcscat(fullPath, L"\\");
        wcscat(fullPath, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Skip directories for safety - only clear files */
            continue;
        }

        /* Remove read-only for deletion */
        SetFileAttributesW(fullPath, FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(fullPath);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

/* Clear temp directories */
static int ClearTempFiles(void) {
    WCHAR tempPath[MAX_PATH];
    WCHAR winTemp[MAX_PATH];
    UINT len;
    int cleared = 0;

    len = GetTempPathW(MAX_PATH, tempPath);
    if (len > 0 && len < MAX_PATH) {
        ClearDirectory(tempPath);
        printf("[+] Cleared user temp: %ls\n", tempPath);
        cleared++;
    }

    len = GetEnvironmentVariableW(L"TMP", tempPath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        ClearDirectory(tempPath);
        cleared++;
    }

    len = GetEnvironmentVariableW(L"TEMP", tempPath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        ClearDirectory(tempPath);
        cleared++;
    }

    if (GetWindowsDirectoryW(winTemp, MAX_PATH) > 0) {
        wcscat(winTemp, L"\\Temp");
        ClearDirectory(winTemp);
        printf("[+] Cleared Windows\\Temp\n");
        cleared++;
    }

    if (cleared > 0) {
        printf("[+] Temp files cleared\n");
        AppendResult(L"• Temp files cleared");
    }
    return 0;
}

/* Empty the Recycle Bin */
static int EmptyRecycleBin(void) {
    HRESULT hr;
    hr = SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (SUCCEEDED(hr)) {
        printf("[+] Recycle Bin emptied\n");
        AppendResult(L"• Recycle Bin emptied");
        return 0;
    }
    printf("[!] Recycle Bin: %s (may be empty)\n", SUCCEEDED(hr) ? "OK" : "failed");
    return 0;
}

/* Flush DNS cache */
static int FlushDnsCache(void) {
    HMODULE hDnsApi = LoadLibraryW(L"dnsapi.dll");
    if (!hDnsApi) {
        printf("[!] Cannot load dnsapi.dll\n");
        return -1;
    }

    typedef BOOL(WINAPI* pDnsFlushResolverCache)(void);
    pDnsFlushResolverCache DnsFlushResolverCache =
        (pDnsFlushResolverCache)(void*)GetProcAddress(hDnsApi, "DnsFlushResolverCache");

    if (!DnsFlushResolverCache) {
        FreeLibrary(hDnsApi);
        printf("[!] DnsFlushResolverCache not found\n");
        return -1;
    }

    if (DnsFlushResolverCache()) {
        printf("[+] DNS cache flushed\n");
        AppendResult(L"• DNS cache flushed");
        FreeLibrary(hDnsApi);
        return 0;
    }
    FreeLibrary(hDnsApi);
    printf("[!] DNS flush failed\n");
    return -1;
}

/* Restart Windows Explorer - fixes sluggish taskbar/desktop */
static int RestartExplorer(void) {
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    DWORD explorerPid = 0;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[!] Cannot enumerate processes (run as Administrator)\n");
        return -1;
    }

    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0) {
                explorerPid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (explorerPid == 0) {
        fprintf(stderr, "[!] Explorer not found\n");
        return -1;
    }

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, explorerPid);
    if (!hProc) {
        fprintf(stderr, "[!] Cannot open Explorer process (run as Administrator)\n");
        return -1;
    }

    if (TerminateProcess(hProc, 0)) {
        printf("[+] Windows Explorer restarted (auto-restarts on Windows 10)\n");
        AppendResult(L"• Windows Explorer restarted");
        CloseHandle(hProc);
        Sleep(2000); /* Give Explorer time to restart */
        return 0;
    }
    CloseHandle(hProc);
    fprintf(stderr, "[!] Failed to restart Explorer\n");
    return -1;
}

/* Optimize screen/display - fixes lagging, stuttering display */
static int OptimizeScreen(BOOL doDwmRestart) {
    HWND hDesktop, hTaskbar;
    int didSomething = 0;

    /* Flush GDI operations - ensures pending graphics commands complete */
    GdiFlush();
    printf("[+] GDI flushed\n");
    AppendResult(L"• GDI flushed");
    didSomething = 1;

    /* Flush DWM composition - syncs desktop window manager */
    {
        HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            typedef HRESULT(WINAPI* pDwmFlush)(void);
            pDwmFlush DwmFlush = (pDwmFlush)(void*)GetProcAddress(hDwm, "DwmFlush");
            if (DwmFlush && SUCCEEDED(DwmFlush())) {
                printf("[+] DWM composition flushed\n");
                AppendResult(L"• DWM composition flushed");
                didSomething = 1;
            }
            FreeLibrary(hDwm);
        }
    }

    /* Force full desktop redraw - clears visual glitches and stale buffers */
    hDesktop = GetDesktopWindow();
    if (hDesktop && RedrawWindow(hDesktop, NULL, NULL,
            RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW)) {
        printf("[+] Desktop redrawn\n");
        AppendResult(L"• Desktop redrawn");
        didSomething = 1;
    }

    /* Redraw taskbar - often a source of lag when stuttering */
    hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hTaskbar && RedrawWindow(hTaskbar, NULL, NULL,
            RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW)) {
        printf("[+] Taskbar redrawn\n");
        AppendResult(L"• Taskbar redrawn");
        didSomething = 1;
    }

    /* Secondary taskbars (multi-monitor) */
    hTaskbar = FindWindowExW(NULL, NULL, L"Shell_SecondaryTrayWnd", NULL);
    while (hTaskbar) {
        RedrawWindow(hTaskbar, NULL, NULL,
            RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
        hTaskbar = FindWindowExW(NULL, hTaskbar, L"Shell_SecondaryTrayWnd", NULL);
    }

    /* Restart DWM - fixes persistent display lag/stutter (brief black screen, needs Admin) */
    if (doDwmRestart) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { 0 };
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, L"dwm.exe") == 0) {
                        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProc) {
                            if (TerminateProcess(hProc, 0)) {
                                printf("[+] DWM restarted (fixes display lag)\n");
                                AppendResult(L"• DWM restarted");
                                CloseHandle(hProc);
                                Sleep(1500);  /* DWM auto-restarts, brief black screen */
                                didSomething = 1;
                            } else {
                                CloseHandle(hProc);
                            }
                            break;
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
    }

    return didSomething ? 0 : -1;
}

/* Clear clipboard - frees memory, minor optimization */
static void ClearClipboard(void) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        CloseClipboard();
        printf("[+] Clipboard cleared\n");
        AppendResult(L"• Clipboard cleared");
    }
}

/* Flush filesystem cache - writes buffered data to disk (needs Admin) */
static int FlushSystemVolume(void) {
    HANDLE hVol = CreateFileW(L"\\\\.\\C:", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[!] Cannot open volume C: (run as Administrator)\n");
        return -1;
    }
    if (FlushFileBuffers(hVol)) {
        printf("[+] Filesystem cache flushed (volume C:)\n");
        AppendResult(L"• Filesystem cache flushed");
        CloseHandle(hVol);
        return 0;
    }
    CloseHandle(hVol);
    fprintf(stderr, "[!] FlushFileBuffers failed\n");
    return -1;
}

/* Trim working set of current process - minor optimization */
static void TrimWorkingSet(void) {
    if (SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1)) {
        printf("[+] Process working set trimmed\n");
        AppendResult(L"• Working set trimmed");
    }
}

static void PrintUsage(const char* prog) {
    printf("Windows 10 Optimizer - Restore system responsiveness\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  (none)     Run all optimizations\n");
    printf("  /temp      Clear temp files only\n");
    printf("  /recycle   Empty Recycle Bin only\n");
    printf("  /dns       Flush DNS cache only\n");
    printf("  /explorer  Restart Windows Explorer only (needs Admin)\n");
    printf("  /screen    Optimize display/screen only (fixes lag)\n");
    printf("  /quick     Skip Explorer & DWM restart (faster, less disruptive)\n");
    printf("  /help      Show this help\n\n");
    printf("For maximum effect, run as Administrator.\n");
}

int main(int argc, char* argv[]) {
    (void)argv;
    int doTemp = 1, doRecycle = 1, doDns = 1, doExplorer = 1, doScreen = 1;
    BOOL doDwmRestart = TRUE;
    int i;
    LPWSTR* argvW;
    int argcW;

    /* Parse wide-char command line for proper Unicode handling */
    argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    if (!argvW) argvW = NULL;

    for (i = 1; i < argc; i++) {
        WCHAR* arg = (argvW && i < argcW) ? argvW[i] : NULL;
        if (arg && (_wcsicmp(arg, L"/help") == 0 || _wcsicmp(arg, L"-h") == 0)) {
            if (argvW) LocalFree(argvW);
            PrintUsage("win_optimizer");
            return 0;
        }
        if (arg && _wcsicmp(arg, L"/temp") == 0) {
            doTemp = 1; doRecycle = 0; doDns = 0; doExplorer = 0; doScreen = 0;
        }
        else if (arg && _wcsicmp(arg, L"/recycle") == 0) {
            doTemp = 0; doRecycle = 1; doDns = 0; doExplorer = 0; doScreen = 0;
        }
        else if (arg && _wcsicmp(arg, L"/dns") == 0) {
            doTemp = 0; doRecycle = 0; doDns = 1; doExplorer = 0; doScreen = 0;
        }
        else if (arg && _wcsicmp(arg, L"/explorer") == 0) {
            doTemp = 0; doRecycle = 0; doDns = 0; doExplorer = 1; doScreen = 0;
        }
        else if (arg && _wcsicmp(arg, L"/screen") == 0) {
            doTemp = 0; doRecycle = 0; doDns = 0; doExplorer = 0; doScreen = 1;
        }
        else if (arg && _wcsicmp(arg, L"/quick") == 0) {
            doExplorer = 0;
            doDwmRestart = FALSE;
        }
    }
    if (argvW) LocalFree(argvW);

    printf("=== Windows 10 Optimizer ===\n\n");

    g_resultBuf[0] = L'\0';

    /* Boost process priority so optimizations complete faster */
    {
        DWORD oldPri = GetPriorityClass(GetCurrentProcess());
        if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
            printf("[+] Process priority boosted\n");
            AppendResult(L"• Process priority boosted");
        }

    if (doTemp) ClearTempFiles();
    if (doRecycle) EmptyRecycleBin();
    if (doDns) FlushDnsCache();
    if (doExplorer) RestartExplorer();
    if (doScreen) OptimizeScreen(doDwmRestart);

    ClearClipboard();
    FlushSystemVolume();
    TrimWorkingSet();

    /* Restore process priority */
    if (oldPri != 0)
        SetPriorityClass(GetCurrentProcess(), oldPri);
    }

    printf("\n[+] Optimization complete.\n");

    /* Show confirmation message box */
    if (wcslen(g_resultBuf) > 0) {
        MessageBoxW(NULL, g_resultBuf, L"Windows 10 Optimizer - Complete",
                    MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
    } else {
        MessageBoxW(NULL, L"No optimizations were performed.",
                    L"Windows 10 Optimizer", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
    }

    return 0;
}
