#include <windows.h>
#include <string>
#include <vector>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    // Attach to parent console if invoked from terminal (CMD/PowerShell)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        MessageBoxW(nullptr, L"Error obteniendo la ruta del ejecutable.", L"Persona 3 Dancing", MB_ICONERROR);
        return 1;
    }

    std::wstring exeDir = exePath;
    size_t slash = exeDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        exeDir = exeDir.substr(0, slash);
    }

    std::wstring binDir = exeDir + L"\\bin";
    std::wstring targetExe = binDir + L"\\p3d_engine.exe";

    // Set DLL directory so Windows loader finds all encapsulated DLLs in bin/
    SetDllDirectoryW(binDir.c_str());

    // Prepend bin/ to PATH
    wchar_t curPath[32768] = {0};
    GetEnvironmentVariableW(L"PATH", curPath, 32768);
    std::wstring newPath = binDir + L";" + curPath;
    SetEnvironmentVariableW(L"PATH", newPath.c_str());

    // Optimization & stability flags
    SetEnvironmentVariableW(L"GOW3_UPSCALER", L"none");
    SetEnvironmentVariableW(L"GOW3_VK_RECORD_THREAD", L"0");
    SetEnvironmentVariableW(L"GOW3_PRESENT_THREAD", L"0");
    SetEnvironmentVariableW(L"__GL_THREADED_OPTIMIZATIONS", L"0");
    SetEnvironmentVariableW(L"DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1", L"1");
    SetEnvironmentVariableW(L"VK_LOADER_DISABLE_INST_EXT_FILTER", L"0");

    // Build command line
    std::wstring cmdLine = L"\"" + targetExe + L"\"";

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            cmdLine += L" ";
            std::wstring arg = argv[i];
            if (arg.find(L' ') != std::wstring::npos) {
                cmdLine += L"\"" + arg + L"\"";
            } else {
                cmdLine += arg;
            }
        }
    } else {
        std::wstring bootBin = exeDir + L"\\out\\p3d_test\\boot-linked.bin";
        std::wstring app0Dir = exeDir + L"\\CUSA12636";
        cmdLine += L" \"" + bootBin + L"\" --app0 \"" + app0Dir + L"\"";
    }

    if (argv) LocalFree(argv);

    std::wstring logPath = exeDir + L"\\game_last_run.log";
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hLog = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si = { sizeof(si) };
    if (hLog != INVALID_HANDLE_VALUE) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hLog;
        si.hStdError = hLog;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr, nullptr,
        TRUE,
        0,
        nullptr,
        exeDir.c_str(),
        &si, &pi);

    if (hLog != INVALID_HANDLE_VALUE) {
        CloseHandle(hLog);
    }

    if (!ok) {
        DWORD err = GetLastError();
        std::wstring msg = L"No se pudo iniciar el motor del juego en:\n" + targetExe +
                           L"\n\nError de Windows: " + std::to_wstring(err);
        MessageBoxW(nullptr, msg.c_str(), L"Persona 3 Dancing - Error de inicio", MB_ICONERROR);
        return 1;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    return static_cast<int>(exitCode);
}
