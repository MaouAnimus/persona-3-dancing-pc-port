#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "../rhythm/rhythm_engine.hpp"

namespace p3d::platform {

struct WindowConfig {
    std::string title{"Persona 3: Dancing in Moonlight (PC Native)"};
    int width{1280};
    int height{720};
    bool fullscreen{false};
};

class NativeWindow {
public:
    using KeyCallback = std::function<void(rhythm::NoteLane lane, bool pressed)>;
    using CommandCallback = std::function<void(int cmd_id)>;
    using RawKeyCallback = std::function<void(int vk, bool pressed)>;

    NativeWindow(const WindowConfig& config);
    ~NativeWindow();

    bool initialize();
    bool poll_events();
    HWND hwnd() const { return hwnd_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool should_close() const { return should_close_; }

    void set_key_callback(KeyCallback cb) { on_key_ = cb; }
    void set_command_callback(CommandCallback cb) { on_command_ = cb; }
    void set_raw_key_callback(RawKeyCallback cb) { on_raw_key_ = cb; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    WindowConfig config_;
    HWND hwnd_{nullptr};
    HINSTANCE hinstance_{nullptr};
    int width_{1280};
    int height_{720};
    bool should_close_{false};
    KeyCallback on_key_{nullptr};
    CommandCallback on_command_{nullptr};
    RawKeyCallback on_raw_key_{nullptr};

    void handle_key(WPARAM wparam, bool is_down);
};

} // namespace p3d::platform
