#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>
#include <functional>
#include "../rhythm/rhythm_engine.hpp"

#pragma comment(lib, "xinput.lib")

namespace p3d::platform {

struct ControllerState {
    bool connected{false};
    XINPUT_STATE state{};
    WORD buttons{0};
    WORD prev_buttons{0};
    float left_stick_x{0.0f};
    float left_stick_y{0.0f};
    float right_stick_x{0.0f};
    float right_stick_y{0.0f};
    float left_trigger{0.0f};
    float right_trigger{0.0f};
};

class InputManager {
public:
    InputManager() = default;

    void update(std::function<void(rhythm::NoteLane, bool)> lane_cb,
                std::function<void(int)> nav_cb) {
        XINPUT_STATE state;
        DWORD res = XInputGetState(0, &state);

        if (res == ERROR_SUCCESS) {
            controller_.connected = true;
            controller_.prev_buttons = controller_.buttons;
            controller_.buttons = state.Gamepad.wButtons;

            if (first_update_) {
                first_update_ = false;
                controller_.prev_buttons = controller_.buttons;
                return;
            }

            // Trigger buttons
            WORD pressed = controller_.buttons & ~controller_.prev_buttons;
            WORD released = ~controller_.buttons & controller_.prev_buttons;

            // Triggers & sticks
            controller_.left_trigger = state.Gamepad.bLeftTrigger / 255.0f;
            controller_.right_trigger = state.Gamepad.bRightTrigger / 255.0f;

            // Rhythm Note Mapping
            // Left Side: D-Pad Up (0), D-Pad Left (1), D-Pad Down (2)
            if (lane_cb) {
                if (pressed & XINPUT_GAMEPAD_DPAD_UP)    lane_cb(rhythm::NoteLane::LeftUp, true);
                if (released & XINPUT_GAMEPAD_DPAD_UP)   lane_cb(rhythm::NoteLane::LeftUp, false);

                if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)  lane_cb(rhythm::NoteLane::LeftLeft, true);
                if (released & XINPUT_GAMEPAD_DPAD_LEFT) lane_cb(rhythm::NoteLane::LeftLeft, false);

                if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)  lane_cb(rhythm::NoteLane::LeftDown, true);
                if (released & XINPUT_GAMEPAD_DPAD_DOWN) lane_cb(rhythm::NoteLane::LeftDown, false);

                // Right Side: Y/Triangle (3), B/Circle (4), A/Cross (5)
                if (pressed & XINPUT_GAMEPAD_Y)          lane_cb(rhythm::NoteLane::RightUp, true);
                if (released & XINPUT_GAMEPAD_Y)         lane_cb(rhythm::NoteLane::RightUp, false);

                if (pressed & XINPUT_GAMEPAD_B)          lane_cb(rhythm::NoteLane::RightRight, true);
                if (released & XINPUT_GAMEPAD_B)         lane_cb(rhythm::NoteLane::RightRight, false);

                if (pressed & XINPUT_GAMEPAD_A)          lane_cb(rhythm::NoteLane::RightDown, true);
                if (released & XINPUT_GAMEPAD_A)         lane_cb(rhythm::NoteLane::RightDown, false);

                // Scratch: Shoulder buttons (LB/RB), triggers, or thumb clicks
                bool scratch_pressed = (pressed & (XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER | 
                                                   XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB)) ||
                                       (controller_.left_trigger > 0.45f || controller_.right_trigger > 0.45f);

                if (scratch_pressed && !prev_scratch_) {
                    lane_cb(rhythm::NoteLane::Scratch, true);
                    prev_scratch_ = true;
                } else if (!scratch_pressed && prev_scratch_) {
                    lane_cb(rhythm::NoteLane::Scratch, false);
                    prev_scratch_ = false;
                }
            }

            // Analog Stick Navigation (edge-detected flicks)
            bool stick_up = (state.Gamepad.sThumbLY > 18000);
            bool stick_down = (state.Gamepad.sThumbLY < -18000);
            bool stick_left = (state.Gamepad.sThumbLX < -18000);
            bool stick_right = (state.Gamepad.sThumbLX > 18000);

            // Navigation Commands (for Song Select and Menus)
            // 1: Up, 2: Down, 3: Left, 4: Right, 5: Confirm (A), 6: Cancel (B), 7: Pause/Start, 8: Restart/Back, 9: Replay (X)
            if (nav_cb) {
                if ((pressed & XINPUT_GAMEPAD_DPAD_UP) || (stick_up && !prev_stick_up_))       nav_cb(1);
                if ((pressed & XINPUT_GAMEPAD_DPAD_DOWN) || (stick_down && !prev_stick_down_)) nav_cb(2);
                if ((pressed & XINPUT_GAMEPAD_DPAD_LEFT) || (stick_left && !prev_stick_left_)) nav_cb(3);
                if ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) || (stick_right && !prev_stick_right_)) nav_cb(4);
                if (pressed & XINPUT_GAMEPAD_A)          nav_cb(5);
                if (pressed & XINPUT_GAMEPAD_B)          nav_cb(6);
                if (pressed & XINPUT_GAMEPAD_START)      nav_cb(7);
                if (pressed & XINPUT_GAMEPAD_BACK)       nav_cb(8);
                if (pressed & XINPUT_GAMEPAD_X)          nav_cb(9);
            }

            prev_stick_up_ = stick_up;
            prev_stick_down_ = stick_down;
            prev_stick_left_ = stick_left;
            prev_stick_right_ = stick_right;
        } else {
            controller_.connected = false;
        }
    }

    bool is_controller_connected() const { return controller_.connected; }

private:
    ControllerState controller_;
    bool first_update_{true};
    bool prev_scratch_{false};
    bool prev_stick_up_{false};
    bool prev_stick_down_{false};
    bool prev_stick_left_{false};
    bool prev_stick_right_{false};
};

} // namespace p3d::platform
