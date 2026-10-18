#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <iostream>

#pragma comment(lib, "winmm.lib")

namespace p3d::audio {

class AudioPlayer {
public:
    AudioPlayer() {
        static int s_counter = 0;
        alias_ = "p3d_bgm_" + std::to_string(++s_counter) + "_" + std::to_string(GetCurrentProcessId());
    }
    ~AudioPlayer() { close(); }

    bool load(const std::string& wav_filepath) {
        close();
        filepath_ = wav_filepath;

        std::string cmd = "open \"" + filepath_ + "\" type waveaudio alias " + alias_;
        MCIERROR err = mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
        if (err != 0) {
            char err_buf[256];
            mciGetErrorStringA(err, err_buf, sizeof(err_buf));
            std::cerr << "[AudioPlayer] Error opening audio: " << err_buf << " (" << filepath_ << ")\n";
            return false;
        }

        std::string set_cmd = "set " + alias_ + " time format milliseconds";
        mciSendStringA(set_cmd.c_str(), nullptr, 0, nullptr);
        is_loaded_ = true;

        // Query length
        char len_buf[64] = {};
        std::string status_cmd = "status " + alias_ + " length";
        mciSendStringA(status_cmd.c_str(), len_buf, sizeof(len_buf), nullptr);
        length_ms_ = std::atof(len_buf);

        std::cout << "[AudioPlayer] Loaded " << filepath_ << " (" << length_ms_ << " ms)\n";
        return true;
    }

    void play() {
        if (!is_loaded_) return;
        std::string cmd = "play " + alias_ + " from 0";
        mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
        is_playing_ = true;
        is_paused_ = false;
    }

    void pause() {
        if (is_playing_ && !is_paused_) {
            std::string cmd = "pause " + alias_;
            mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
            is_paused_ = true;
        }
    }

    void resume() {
        if (is_playing_ && is_paused_) {
            std::string cmd = "resume " + alias_;
            mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
            is_paused_ = false;
        }
    }

    void stop() {
        if (is_loaded_) {
            std::string cmd = "stop " + alias_;
            mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
            is_playing_ = false;
            is_paused_ = false;
        }
    }

    void close() {
        if (is_loaded_) {
            std::string cmd = "close " + alias_;
            mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
            is_loaded_ = false;
            is_playing_ = false;
            is_paused_ = false;
        }
    }

    // Queries current playback position in milliseconds directly from audio device
    double get_position_ms() const {
        if (!is_playing_ || is_paused_) return 0.0;
        char pos_buf[64] = {};
        std::string cmd = "status " + alias_ + " position";
        MCIERROR err = mciSendStringA(cmd.c_str(), pos_buf, sizeof(pos_buf), nullptr);
        if (err == 0) {
            return std::atof(pos_buf);
        }
        return 0.0;
    }

    double length_ms() const { return length_ms_; }
    bool is_playing() const { return is_playing_ && !is_paused_; }
    bool is_paused() const { return is_paused_; }

private:
    std::string alias_{"p3d_bgm"};
    std::string filepath_{};
    bool is_loaded_{false};
    bool is_playing_{false};
    bool is_paused_{false};
    double length_ms_{0.0};
};

} // namespace p3d::audio
