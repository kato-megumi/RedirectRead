#include <Windows.h>
#include <detours.h>
#include <iostream>
#include <fstream>
#include <string>

#define DEBUG 0

#if DEBUG
static FILE* streamconsole;
DWORD WINAPI SetConsoleTop(LPVOID lpParameter)
{
    WCHAR consoleTitle[256] = { 0 };

    while (true)
    {
        GetConsoleTitleW(consoleTitle, 256);
        HWND hConsole = FindWindowW(NULL, (LPWSTR)consoleTitle);
        if (hConsole != NULL)
        {
            SetWindowPos(hConsole, HWND_TOPMOST, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            break;
        }
    }

    return 0;
}

VOID WINAPI SetConsole()
{
    DWORD mode = 0;
    HANDLE hStdin = 0;

    AllocConsole();
    AttachConsole(ATTACH_PARENT_PROCESS);
    freopen_s(&streamconsole, "CONIN$", "r+t", stdin);
    freopen_s(&streamconsole, "CONOUT$", "w+t", stdout);
    SetConsoleTitleW(L"CmvsFileHook");

    CreateThread(NULL, NULL, SetConsoleTop, NULL, NULL, NULL);

    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & ~ENABLE_QUICK_EDIT_MODE);

    //system("chcp 65001");
    //setlocale(LC_ALL, "chs");

    std::locale::global(std::locale(""));
}
#endif

// Function to read the target base path from a config file
std::pair<std::wstring, std::wstring> ReadPathsFromConfig(const std::wstring& configFilePath) {
    std::wifstream configFile(configFilePath);
    std::wstring targetBasePath;
    std::wstring redirectBasePath;
    if (configFile.is_open()) {
        std::getline(configFile, targetBasePath);
        std::getline(configFile, redirectBasePath);
        configFile.close();
    }
    return { targetBasePath, redirectBasePath };
}
//Normalize and expand environment variables
std::wstring NormalizePath(const std::wstring& inputPath) {
    WCHAR expandedPath[MAX_PATH];
    if (!ExpandEnvironmentStringsW(inputPath.c_str(), expandedPath, MAX_PATH)) {
        std::wcerr << L"Failed to expand environment strings. Error: " << GetLastError() << std::endl;
        return L"";
    }

    WCHAR normalizedPath[MAX_PATH];
    if (!GetFullPathNameW(expandedPath, MAX_PATH, normalizedPath, nullptr)) {
        std::wcerr << L"Failed to normalize path. Error: " << GetLastError() << std::endl;
        return L"";
    }

    return normalizedPath;
}

// Pointer to the original CreateFileW function
typedef HANDLE(WINAPI* CreateFileW_t)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    );

CreateFileW_t OriginalCreateFileW = nullptr;

// Define the base path to redirect
std::wstring BASE_PATH;
std::wstring REDIRECT_BASE_PATH;

// Helper function to get the current directory
std::wstring GetCurrentDirectoryWString() {
    wchar_t buffer[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, buffer);
    return std::wstring(buffer);
}

// Hooked CreateFileW function
HANDLE WINAPI HookedCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    if (lpFileName) {
        // Expand environment variables in the file name
		std::wstring fullPath = NormalizePath(lpFileName);
		std::wstring basePath = NormalizePath(BASE_PATH);
		std::wstring redirectedBasePath = NormalizePath(REDIRECT_BASE_PATH);

        // Check if the path starts with the target base path
        if (fullPath.rfind(basePath, 0) == 0) {
            // Get the relative path
            std::wstring relativePath = fullPath.substr(basePath.length());

            // Construct the new path
            std::wstring redirectedPath = redirectedBasePath + L"\\" + relativePath;

            // Replace the file path with the redirected path
            lpFileName = redirectedPath.c_str();
#if DEBUG
            std::wcout << "redirectedPath: " << redirectedPath << "\n-redirectedBasePath: " << redirectedBasePath << "\n-relativePath: " << relativePath << std::endl;
#endif
        }
    }

    // Call the original CreateFileW function
    return OriginalCreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );
}

VOID StartHook()
{
#if DEBUG
    SetConsole();
#endif
    OriginalCreateFileW = (CreateFileW_t)DetourFindFunction("kernel32.dll", "CreateFileW");

    // Load the target base path from the config file
    auto paths = ReadPathsFromConfig(L"hook.conf");
    BASE_PATH = paths.first.empty() ? L"%appdata%" : paths.first;
    REDIRECT_BASE_PATH = paths.second.empty() ? GetCurrentDirectoryWString() : paths.second;


    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)OriginalCreateFileW, HookedCreateFileW);
    if (DetourTransactionCommit() != NOERROR)
        MessageBox(0, L"Start Hook error.", L"Start hooking", 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		StartHook();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" VOID __declspec(dllexport) dummy() {}
