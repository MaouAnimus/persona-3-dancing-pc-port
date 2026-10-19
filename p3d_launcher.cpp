#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// Función auxiliar para leer claves simples de p3d.ini
std::wstring ReadIniValue(const std::wstring& iniPath, const std::wstring& searchKey, const std::wstring& defaultValue) {
    std::wifstream file(iniPath.c_str());
    if (!file.is_open()) return defaultValue;

    std::wstring line;
    while (std::getline(file, line)) {
        // Trim
        size_t first = line.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos || line[first] == L'#') continue;

        size_t eq = line.find(L'=', first);
        if (eq == std::wstring::npos) continue;

        std::wstring key = line.substr(first, eq - first);
        // Trim key
        size_t keyEnd = key.find_last_not_of(L" \t");
        if (keyEnd != std::wstring::npos) key = key.substr(0, keyEnd + 1);

        if (_wcsicmp(key.c_str(), searchKey.c_str()) == 0) {
            std::wstring val = line.substr(eq + 1);
            size_t valStart = val.find_first_not_of(L" \t");
            size_t valEnd = val.find_last_not_of(L" \t\r\n");
            if (valStart != std::wstring::npos && valEnd != std::wstring::npos) {
                return val.substr(valStart, valEnd - valStart + 1);
            }
            return L"";
        }
    }
    return defaultValue;
}

bool TryLaunchGui(const std::wstring& exeDir) {
    std::wstring pywPath = exeDir + L"\\p3d_launcher.pyw";
    std::wstring pyPath = exeDir + L"\\p3d_launcher.py";
    std::wstring settingsPath = exeDir + L"\\p3d_settings.py";

    if (GetFileAttributesW(pywPath.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(pyPath.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(settingsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    // Intentar pythonw primero para evitar ventana negra de CMD
    std::wstring cmdW = L"pythonw \"" + pywPath + L"\"";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (CreateProcessW(nullptr, &cmdW[0], nullptr, nullptr, FALSE, 0, nullptr, exeDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    // Intentar python estándar
    std::wstring cmdPy = L"python \"" + pyPath + L"\"";
    if (CreateProcessW(nullptr, &cmdPy[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, exeDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    return false;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    // Conectar a consola padre si se ejecuta desde CMD o PowerShell
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

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool forceGui = false;
    std::vector<std::wstring> engineArgs;
#ifdef P3D_DIRECT_LAUNCH
    bool forcePlay = true;
#else
    bool forcePlay = false;
#endif

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--launcher" || arg == L"-l" || arg == L"--gui") {
                forceGui = true;
            } else if (arg == L"--play" || arg == L"-p" || arg == L"--direct") {
                forcePlay = true;
            } else {
                engineArgs.push_back(arg);
            }
        }
    }

    // Si se hace doble clic en el ejecutable sin argumentos o con --launcher, abrimos el Launcher GUI
    if ((argc <= 1 && !forcePlay) || forceGui) {
        if (TryLaunchGui(exeDir)) {
            if (argv) LocalFree(argv);
            return 0;
        }
        MessageBoxW(nullptr,
            L"No se pudo abrir el launcher. Comprueba que p3d_launcher.py y "
            L"p3d_launcher.pyw y p3d_settings.py esten junto al ejecutable y que Python con PySide6 "
            L"este instalado y disponible en PATH.",
            L"Persona 3 Dancing - Launcher", MB_ICONERROR);
        if (argv) LocalFree(argv);
        return 1;
    }

    std::wstring binDir = exeDir + L"\\bin";
    std::wstring targetExe = binDir + L"\\p3d_engine.exe";
    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        targetExe = exeDir + L"\\p3d.exe";
    }
    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(nullptr, L"No se encontro bin\\p3d_engine.exe ni p3d.exe.",
                    L"Persona 3 Dancing - Motor", MB_ICONERROR);
        if (argv) LocalFree(argv);
        return 1;
    }

    // Configurar rutas DLL de Windows
    SetDllDirectoryW(binDir.c_str());

    // Añadir bin/ y clang64 al PATH
    wchar_t curPath[32768] = {0};
    GetEnvironmentVariableW(L"PATH", curPath, 32768);
    std::wstring newPath = binDir + L";C:\\msys64\\clang64\\bin;" + curPath;
    SetEnvironmentVariableW(L"PATH", newPath.c_str());

    // Cargar p3d.ini si existe
    std::wstring iniPath = exeDir + L"\\p3d.ini";
    SetEnvironmentVariableW(L"GOW3_CONFIG", iniPath.c_str());

    SetEnvironmentVariableW(L"GOW3_UPSCALER", L"off");
    SetEnvironmentVariableW(L"GOW3_FSR1", L"0");
    SetEnvironmentVariableW(L"GOW3_RCAS", ReadIniValue(iniPath, L"rcas", L"1").c_str());
    SetEnvironmentVariableW(L"GOW3_ANISOTROPY", ReadIniValue(iniPath, L"anisotropy", L"8").c_str());
    SetEnvironmentVariableW(L"GOW3_FRAMES_AHEAD", ReadIniValue(iniPath, L"frames_queued", L"1").c_str());
    SetEnvironmentVariableW(L"GOW3_ASYNC_SHADERS", ReadIniValue(iniPath, L"async_shaders", L"1").c_str());
    std::wstring fps = ReadIniValue(iniPath, L"fps_limit", L"60");
    if (fps != L"30" && fps != L"60") fps = L"60";
    SetEnvironmentVariableW(L"GOW3_FPS_LIMIT", fps.c_str());
    SetEnvironmentVariableW(L"GOW3_VBLANK_HZ", L"60");
    SetEnvironmentVariableW(L"GOW3_ENGINE_FPS", L"60");
    SetEnvironmentVariableW(L"GOW3_GPU_USER_DIR", (exeDir + L"\\user").c_str());
    for (const auto key : {L"GOW3_FULLSCREEN", L"GOW3_RENDER_RES", L"GOW3_INTERNAL_WIDTH", L"GOW3_INTERNAL_HEIGHT"}) {
        SetEnvironmentVariableW(key, nullptr);
    }

    // Banderas de optimización y estabilidad Vulkan
    SetEnvironmentVariableW(L"GOW3_VK_RECORD_THREAD", L"0");
    SetEnvironmentVariableW(L"GOW3_PRESENT_THREAD", L"0");
    SetEnvironmentVariableW(L"GOW3_DRAW_PIPE", L"0");
    SetEnvironmentVariableW(L"GOW3_PREP_WORKERS", L"0");
    SetEnvironmentVariableW(L"__GL_THREADED_OPTIMIZATIONS", L"0");
    SetEnvironmentVariableW(L"DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1", L"1");
    SetEnvironmentVariableW(L"VK_LOADER_DISABLE_INST_EXT_FILTER", L"0");

    // Construir línea de comandos
    std::wstring cmdLine = L"\"" + targetExe + L"\"";

    if (!engineArgs.empty() && !engineArgs.front().empty() && engineArgs.front()[0] != L'-') {
        for (const auto& arg : engineArgs) {
            cmdLine += L" ";
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
        for (const auto& arg : engineArgs) {
            cmdLine += L" \"" + arg + L"\"";
        }
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
        CREATE_NO_WINDOW,
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
