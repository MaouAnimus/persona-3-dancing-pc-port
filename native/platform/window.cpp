#include "window.hpp"
#include <iostream>

namespace p3d::platform {

static NativeWindow* s_window_instance = nullptr;

NativeWindow::NativeWindow(const WindowConfig& config)
    : config_(config), width_(config.width), height_(config.height) {
    s_window_instance = this;
}

NativeWindow::~NativeWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassA("P3DNativeWindowClass", hinstance_);
}

bool NativeWindow::initialize() {
    hinstance_ = GetModuleHandle(nullptr);

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "P3DNativeWindowClass";

    if (!RegisterClassExA(&wc)) {
        std::cerr << "[NativeWindow] Failed to register window class\n";
        return false;
    }

    RECT rc = {0, 0, width_, height_};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExA(
        0,
        "P3DNativeWindowClass",
        config_.title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr,
        hinstance_,
        this
    );

    if (!hwnd_) {
        std::cerr << "[NativeWindow] Failed to create Win32 window\n";
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

bool NativeWindow::poll_events() {
    MSG msg = {};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            should_close_ = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return !should_close_;
}

void NativeWindow::handle_key(WPARAM wparam, bool is_down) {
    if (on_raw_key_) {
        on_raw_key_(static_cast<int>(wparam), is_down);
    }

    // Mappings:
    // Left side: W (Up), A (Left), S (Down)
    // Right side: I or Up Arrow (Triangle), L or Right Arrow (Circle), K or Down Arrow (Cross)
    // Scratch: Space or Shift
    if (on_key_) {
        switch (wparam) {
            case 'W': on_key_(rhythm::NoteLane::LeftUp, is_down); break;
            case 'A': on_key_(rhythm::NoteLane::LeftLeft, is_down); break;
            case 'S': on_key_(rhythm::NoteLane::LeftDown, is_down); break;
            case 'I':
            case VK_UP: on_key_(rhythm::NoteLane::RightUp, is_down); break;
            case 'L':
            case VK_RIGHT: on_key_(rhythm::NoteLane::RightRight, is_down); break;
            case 'K':
            case VK_DOWN: on_key_(rhythm::NoteLane::RightDown, is_down); break;
            case VK_SPACE:
            case VK_SHIFT: on_key_(rhythm::NoteLane::Scratch, is_down); break;
            default: break;
        }
    }

    // Command shortcuts
    if (is_down && on_command_) {
        if (wparam == VK_ESCAPE) on_command_(1); // Pause / Menu
        if (wparam == 'R')       on_command_(2); // Restart track
    }
}

LRESULT CALLBACK NativeWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (s_window_instance) {
        switch (msg) {
            case WM_CLOSE:
                s_window_instance->should_close_ = true;
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_KEYDOWN:
                if ((lparam & (1 << 30)) == 0) { // First press only (no auto-repeat)
                    s_window_instance->handle_key(wparam, true);
                }
                return 0;
            case WM_KEYUP:
                s_window_instance->handle_key(wparam, false);
                return 0;
            case WM_LBUTTONDOWN:
                if (s_window_instance->on_raw_key_) {
                    s_window_instance->on_raw_key_(VK_RETURN, true);
                }
                if (s_window_instance->on_key_) {
                    s_window_instance->on_key_(rhythm::NoteLane::Scratch, true);
                }
                return 0;
            case WM_LBUTTONUP:
                if (s_window_instance->on_key_) {
                    s_window_instance->on_key_(rhythm::NoteLane::Scratch, false);
                }
                return 0;
            case WM_RBUTTONDOWN:
                if (s_window_instance->on_raw_key_) {
                    s_window_instance->on_raw_key_(VK_ESCAPE, true);
                }
                return 0;
            case WM_SIZE:
                s_window_instance->width_ = LOWORD(lparam);
                s_window_instance->height_ = HIWORD(lparam);
                return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

} // namespace p3d::platform
